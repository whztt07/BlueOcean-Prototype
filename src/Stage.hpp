﻿#pragma once

//
// 広大なステージ
//  高さ情報を持つだけでは表現力に限界がある
//  効率的に複雑な地形を表現するには..
//

#include <vector>
#include <cinder/Perlin.h>
#include <cinder/Color.h>
#include <cinder/TriMesh.h>
#include <cinder/AxisAlignedBox.h>
#include <cinder/Rand.h>
#include "StageObj.hpp"
#include "StageObjFactory.hpp"
#include <glm/gtc/noise.hpp>


namespace ngs {

class Stage {
  ci::ivec2 size_;

  std::vector<std::vector<int>> height_map_;
  
  ci::TriMesh land_;
  ci::AxisAlignedBox aabb_;

  std::vector<StageObj> stage_objects_;


  void createStageObjects(const int width, const int deep,
                          const StageObjFactory& factory) {
    for (int z = 0; z < deep; ++z) {
      for (int x = 0; x < width; ++x) {
        int y = height_map_[z][x];
        auto stageobj = factory.create(y);
        if (!stageobj.first) continue;

        stage_objects_.emplace_back(stageobj.second, ci::vec3(x + 0.5f, y, z + 0.5f), ci::vec3(0), ci::vec3(1.0f / 16.0f));
      }
    }
  }

  
public:
  // FIXME:奥行きはdeepなのか??
  Stage(const int width, const int deep,
        const int offset_x, const int offset_z,
        const ci::Perlin& random,
        const StageObjFactory& factory,
        const ci::vec3& random_scale)
    : size_(width, deep)
  {
    height_map_.resize(deep + 2);
    for (auto& row : height_map_) {
      row.resize(width + 2);
    }

    // パーリンノイズを使っていい感じに地形の起伏を生成
    // 周囲１ブロックを余計に生成している
    for (int z = -1; z < (deep + 1); ++z) {
      for (int x = -1; x < (width + 1); ++x) {
        ci::vec2 ofs((x + offset_x * width) * random_scale.x,
                     (z + offset_z * deep) * random_scale.x);

        float height = random.fBm(ofs);
        float scale  = (glm::simplex(ofs * random_scale.y) + 1.0f) * random_scale.z;
        
        height_map_[z + 1][x + 1] = glm::clamp(height * scale + 2.0f, 0.0f, 16.0f);
      }
    }

    // 高さ情報を元にTriMeshを生成
    // 隣のブロックの高さを調べ、自分より低ければその分壁を作る作戦
    // TODO:コピペ感をなくす
    uint32_t index = 0;
    for (int z = 0; z < deep; ++z) {
      for (int x = 0; x < width; ++x) {
        float y = height_map_[z + 1][x + 1];
        
        {
          // 上面
          ci::vec3 p[] = {
            {     x, y, z },
            { x + 1, y, z },
            {     x, y, z + 1 },
            { x + 1, y, z + 1 },
          };

          // ４方向で高い場所がある場合法線が短くなる
          float n0 = 1.0;
          float n1 = 1.0;
          float n2 = 1.0;
          float n3 = 1.0;

          if (height_map_[z + 1 - 1][x + 1] > y) {
            n0 *= 0.75;
            n1 *= 0.75;
          }
          if (height_map_[z + 1 + 1][x + 1] > y) {
            n2 *= 0.75;
            n3 *= 0.75;
          }
          if (height_map_[z + 1][x + 1 - 1] > y) {
            n0 *= 0.75;
            n2 *= 0.75;
          }
          if (height_map_[z + 1][x + 1 + 1] > y) {
            n1 *= 0.75;
            n3 *= 0.75;
          }

          if (height_map_[z + 1 - 1][x + 1 - 1] > y) {
            n0 *= 0.75;
          }
          if (height_map_[z + 1 + 1][x + 1 - 1] > y) {
            n2 *= 0.75;
          }
          if (height_map_[z + 1 - 1][x + 1 + 1] > y) {
            n1 *= 0.75;
          }
          if (height_map_[z + 1 + 1][x + 1 + 1] > y) {
            n3 *= 0.75;
          }
          
          
          ci::vec3 n[] = {
            { 0, n0, 0 },
            { 0, n1, 0 },
            { 0, n2, 0 },
            { 0, n3, 0 },
          };
          
          ci::vec2 uv[] = {
            { 0, p[0].y / 16.0f },
            { 0, p[1].y / 16.0f },
            { 0, p[2].y / 16.0f },
            { 0, p[3].y / 16.0f },
          };
        
          land_.appendPositions(&p[0], 4);
          land_.appendNormals(&n[0], 4);
          land_.appendTexCoords0(&uv[0], 4);
        
          land_.appendTriangle(index + 0, index + 2, index + 1);
          land_.appendTriangle(index + 1, index + 2, index + 3);
          index += 4;
        }

        if ((height_map_[z - 1 + 1][x + 1] < y)) {
          // 側面(z-)
          int dy = y - height_map_[z - 1 + 1][x + 1];
          int xl_h = height_map_[z - 1 + 1][x + 1 - 1];
          int xr_h = height_map_[z - 1 + 1][x + 1 + 1];
          
          for (int h = 0; h < dy; ++h) {
            ci::vec3 p[] = {
              {     x,     y - h, z },
              { x + 1,     y - h, z },
              {     x, y - 1 - h, z },
              { x + 1, y - 1 - h, z },
            };

            float n0 = -1.0;
            float n1 = -1.0;
            float n2 = -1.0;
            float n3 = -1.0;
            if (h == (dy - 1)) {
              n2 *= 0.6;
              n3 *= 0.6;
            }
            if (xl_h >= (y - h)) {
              n0 *= 0.6;
            }
            if (xr_h >= (y - h)) {
              n1 *= 0.6;
            }
            if (xl_h >= (y - h - 1)) {
              n2 *= 0.6;
            }
            if (xr_h >= (y - h - 1)) {
              n3 *= 0.6;
            }
            
            ci::vec3 n[] = {
              { 0, 0, n0 },
              { 0, 0, n1 },
              { 0, 0, n2 },
              { 0, 0, n3 },
            };

            ci::vec2 uv[] = {
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
            };
          
            land_.appendPositions(&p[0], 4);
            land_.appendNormals(&n[0], 4);
            land_.appendTexCoords0(&uv[0], 4);
        
            land_.appendTriangle(index + 0, index + 1, index + 2);
            land_.appendTriangle(index + 1, index + 3, index + 2);
            index += 4;
          }
        }
        
        if ((height_map_[z + 1 + 1][x + 1] < y)) {
          // 側面(z+)
          int dy = y - height_map_[z + 1 + 1][x + 1];
          int xl_h = height_map_[z + 1 + 1][x + 1 - 1];
          int xr_h = height_map_[z + 1 + 1][x + 1 + 1];

          for (int h = 0; h < dy; ++h) {
            ci::vec3 p[] = {
              {     x,     y - h, z + 1 },
              { x + 1,     y - h, z + 1 },
              {     x, y - 1 - h, z + 1 },
              { x + 1, y - 1 - h, z + 1 },
            };

            float n0 = 1.0;
            float n1 = 1.0;
            float n2 = 1.0;
            float n3 = 1.0;
            if (h == (dy - 1)) {
              n2 *= 0.6;
              n3 *= 0.6;
            }
            if (xl_h >= (y - h)) {
              n0 *= 0.6;
            }
            if (xr_h >= (y - h)) {
              n1 *= 0.6;
            }
            if (xl_h >= (y - h - 1)) {
              n2 *= 0.6;
            }
            if (xr_h >= (y - h - 1)) {
              n3 *= 0.6;
            }
            
            ci::vec3 n[] = {
              { 0, 0, n0 },
              { 0, 0, n1 },
              { 0, 0, n2 },
              { 0, 0, n3 },
            };

            ci::vec2 uv[] = {
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
            };
          
            land_.appendPositions(&p[0], 4);
            land_.appendNormals(&n[0], 4);
            land_.appendTexCoords0(&uv[0], 4);
        
            land_.appendTriangle(index + 0, index + 3, index + 1);
            land_.appendTriangle(index + 0, index + 2, index + 3);
            index += 4;
          }
        }
        
        if ((height_map_[z + 1][x - 1 + 1] < y)) {
          // 側面(x-)
          int dy = y - height_map_[z + 1][x - 1 + 1];
          int zl_h = height_map_[z + 1 - 1][x - 1 + 1];
          int zr_h = height_map_[z + 1 + 1][x - 1 + 1];

          for (int h = 0; h < dy; ++h) {
            ci::vec3 p[] = {
              { x,     y - h,     z },
              { x,     y - h, z + 1 },
              { x, y - 1 - h,     z },
              { x, y - 1 - h, z + 1 },
            };

            float n0 = -1.0;
            float n1 = -1.0;
            float n2 = -1.0;
            float n3 = -1.0;
            if (h == (dy - 1)) {
              n2 *= 0.6;
              n3 *= 0.6;
            }
            if (zl_h >= (y - h)) {
              n0 *= 0.6;
            }
            if (zr_h >= (y - h)) {
              n1 *= 0.6;
            }
            if (zl_h >= (y - h - 1)) {
              n2 *= 0.6;
            }
            if (zr_h >= (y - h - 1)) {
              n3 *= 0.6;
            }
          
            ci::vec3 n[] = {
              { n0, 0, 0 },
              { n1, 0, 0 },
              { n2, 0, 0 },
              { n3, 0, 0 },
            };

            ci::vec2 uv[] = {
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
            };
          
            land_.appendPositions(&p[0], 4);
            land_.appendNormals(&n[0], 4);
            land_.appendTexCoords0(&uv[0], 4);
        
            land_.appendTriangle(index + 0, index + 2, index + 1);
            land_.appendTriangle(index + 1, index + 2, index + 3);
            index += 4;
          }
        }
        
        if ((height_map_[z + 1][x + 1 + 1] < y)) {
          // 側面(x+)
          int dy = y - height_map_[z + 1][x + 1 + 1];
          int zl_h = height_map_[z + 1 - 1][x + 1 + 1];
          int zr_h = height_map_[z + 1 + 1][x + 1 + 1];

          for (int h = 0; h < dy; ++h) {
            ci::vec3 p[] = {
              { x + 1,     y - h,     z },
              { x + 1,     y - h, z + 1 },
              { x + 1, y - 1 - h,     z },
              { x + 1, y - 1 - h, z + 1 },
            };

            float n0 = 1.0;
            float n1 = 1.0;
            float n2 = 1.0;
            float n3 = 1.0;
            if (h == (dy - 1)) {
              n2 *= 0.6;
              n3 *= 0.6;
            }
            if (zl_h >= (y - h)) {
              n0 *= 0.6;
            }
            if (zr_h >= (y - h)) {
              n1 *= 0.6;
            }
            if (zl_h >= (y - h - 1)) {
              n2 *= 0.6;
            }
            if (zr_h >= (y - h - 1)) {
              n3 *= 0.6;
            }

            ci::vec3 n[] = {
              { n0, 0, 0 },
              { n1, 0, 0 },
              { n2, 0, 0 },
              { n3, 0, 0 },
            };

            ci::vec2 uv[] = {
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
              { 0, p[0].y / 16.0f },
            };
          
            land_.appendPositions(&p[0], 4);
            land_.appendNormals(&n[0], 4);
            land_.appendTexCoords0(&uv[0], 4);
        
            land_.appendTriangle(index + 0, index + 1, index + 3);
            land_.appendTriangle(index + 0, index + 3, index + 2);
            index += 4;
          }
        }
      }
    }
    aabb_ = land_.calcBoundingBox();

    // 周囲1ピクセル余分に生成していた分を取り除く
    height_map_.erase(std::begin(height_map_));
    height_map_.pop_back();
    for (auto& row : height_map_) {
      row.erase(std::begin(row));
      row.pop_back();
    }

    // ステージ上に乗っかっているオブジェクトを生成
    // createStageObjects(width, deep, factory);
  }

  
  const std::vector<std::vector<int>>& getHeightMap() const {
    return height_map_;
  }
  
  const ci::TriMesh& getLandMesh() const {
    return land_;
  }

  const ci::AxisAlignedBox& getAABB() const {
    return aabb_;
  }
  
  const ci::ivec2& getSize() const {
    return size_;
  }

  const std::vector<StageObj> getStageObjects() const {
    return stage_objects_;
  }

};

}
