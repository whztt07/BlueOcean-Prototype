﻿#pragma once

//
// 見つけたアイテムを報告する画面
//

#include <cinder/ObjLoader.h>
#include <cinder/Easing.h>
#include <cinder/Timeline.h>
#include <cinder/Tween.h>
#include "Item.hpp"
#include "AudioEvent.hpp"


namespace ngs {

class ItemReporter {
  Event& event_;
  
  // アイテム表示用専用カメラ
  ci::CameraPersp camera_;
  float fov_;
  float near_z_;

  Light light_;

  ci::gl::GlslProgRef shader_;
  ci::gl::VboMeshRef  model_[2];

  ci::AxisAlignedBox aabb_;
  
  ci::vec3 bg_translate_;
  ci::vec3 new_translate_;
  
  Item item_;
  bool new_item_;
  
  uint32_t touch_id_;
  bool draged_;

  ci::quat drag_rotate_;
  ci::vec3 center_;
  ci::vec3 offset_;

  ci::TimelineRef timeline_;
  ci::Anim<float> tween_scale_;

  // UI有効
  bool active_ = false;

  bool   first_update_ = true;
  double current_time_ = 0.0;
  
  
public:
  ItemReporter(Event& event,
               const ci::JsonTree& params,
               const ci::TimelineRef& timeline)
    : event_(event),
      fov_(params.getValueForKey<float>("camera.fov")),
      near_z_(params.getValueForKey<float>("camera.near_z")),
      light_(createLight(params["light"])),
      bg_translate_(Json::getVec<ci::vec3>(params["bg_translate"])),
      new_translate_(Json::getVec<ci::vec3>(params["new_translate"])),
      draged_(false),
      drag_rotate_(Json::getQuat(params["drag_rotate"])),
      center_(Json::getVec<ci::vec3>(params["center"])),
      timeline_(ci::Timeline::create()),
      tween_scale_(0.0f)
  {
    int width  = ci::app::getWindowWidth();
    int height = ci::app::getWindowHeight();
    // FIXME:生成時に画角を考慮する必要がある
    float fov = getVerticalFov(width / float(height), fov_, near_z_);
    
    camera_ = ci::CameraPersp(width, height,
                              fov,
                              near_z_,
                              params.getValueForKey<float>("camera.far_z"));

    camera_.setEyePoint(Json::getVec<ci::vec3>(params["camera.position"]));
    camera_.setViewDirection(Json::getVec<ci::vec3>(params["camera.direction"]));

    shader_ = createShader("color", "color");

    ci::TriMesh mesh(PLY::load("item_reporter.ply"));

    // 少しオフセットを加えたAABBをクリック判定に使う
    ci::mat4 transform = glm::translate(bg_translate_);
    auto bb = mesh.calcBoundingBox();
    aabb_ = ci::AxisAlignedBox(bb.getMin(),
                               bb.getMax() + ci::vec3(0, -31, 0)).transformed(transform);

    model_[0] = ci::gl::VboMesh::create(mesh);
    model_[1] = ci::gl::VboMesh::create(PLY::load("new.ply"));

    // 親のタイムラインに接続
    // timeline->add(timeline_);
    
    // 開始演出
    AudioEvent::play(event_, "item_found");
    timeline_->apply(&tween_scale_, 0.0f, 1.0f, 0.5f, ci::EaseOutBack())
      .finishFn([this]() {
          active_ = true;
        });
  }

  // ~ItemReporter() {
  //   // 親から取り除く
  //   timeline_->removeSelf();
  // }


  void loadItem(const ci::JsonTree& params, bool new_item = false) {
    item_ = Item(params);

    const auto& aabb = item_.getAABB();
    offset_ = -(aabb.getMin() + aabb.getMax()) / 2.0f;

    new_item_ = new_item;
  }


  void touchesBegan(const int touching_num, const std::vector<Touch>& touches) {
#if defined (CINDER_COCOA_TOUCH)
    // iOS版は最初のを覚えとく
    if (touching_num == 1) {
      touch_id_ = touches[0].getId();
      draged_   = false;
    }
#else
    // PC版はMouseDownを覚えておく
    if ((touches.size() == 1) && touches[0].isMouse()) {
      touch_id_ = touches[0].getId();
      draged_   = false;
    }
#endif
  }

  void touchesMoved(const int touching_num, const std::vector<Touch>& touches) {
#if defined (CINDER_COCOA_TOUCH)
    // iOS版はシングルタッチ操作
    if (touching_num == 1 && touches.size() == 1)
#else
      // PCはマウス操作で回転
    if ((touches.size() == 1) && touches[0].isMouse())
#endif
    {
      ci::vec2 d{ touches[0].getPos() -  touches[0].getPrevPos() };
      float l = length(d);
      if (l > 0.0f) {
        d = normalize(d);
        ci::vec3 v(d.y, d.x, 0.0f);
        ci::quat r = glm::angleAxis(l * 0.01f, v);
        drag_rotate_ = r * drag_rotate_;
        
        draged_ = true;
      }
    }
  }

  void touchesEnded(const int touching_num, const std::vector<Touch>& touches) {
    if (!active_) return;

    bool touch_up = false;
#if defined (CINDER_COCOA_TOUCH)
    // iOS版は最初のタッチが終わったら
    if (!draged_
        && (touching_num == 0)
        && (touches.size() == 1)
        && touches[0].getId() == touch_id_) {
      touch_up = true;
    }
#else
    // PC版はMouseUpで判定
    if (!draged_
        && (touches.size() == 1)
        && (touches[0].isMouse())) {
      touch_up = true;
    }
#endif

    if (touch_up) {
      // スクリーン座標→正規化座標
      ci::vec2 pos = touches[0].getPos();
      float sx = pos.x / ci::app::getWindowWidth();
      float sy = 1.0f - pos.y / ci::app::getWindowHeight();
    
      ci::Ray ray = camera_.generateRay(sx, sy,
                                        camera_.getAspectRatio());

      if (aabb_.intersects(ray)) {
        // 終了
        DOUT << "Finish item reporter." << std::endl;

        active_ = false;
        // 終了演出
        AudioEvent::play(event_, "agree");
        timeline_->apply(&tween_scale_, 0.0f, 0.5f, ci::EaseInBack())
          .finishFn([this]() {
              event_.signal("close_item_reporter");
            });
      }
    
      draged_ = false;
    }
  }

  
  void resize(const float aspect) {
    camera_.setAspectRatio(aspect);
    camera_.setFov(getVerticalFov(aspect, fov_, near_z_));
  }

  
  void update() {
    if (first_update_) {
      // TIPS:コンストラクタに重い処理が集中しているので
      //      初回更新時からタイムラインの時間を進め始める
      current_time_ = ci::app::getElapsedSeconds();
      first_update_ = false;
    }

    double ct = ci::app::getElapsedSeconds() - current_time_;
    timeline_->step(ct);

    current_time_ += ct;
  }

  void draw() {
    auto vp = ci::gl::getViewport();

    ci::vec2 size(ci::vec2(vp.second) * tween_scale_());
    ci::vec2 offset((ci::vec2(vp.second) - size) / 2.0f);

    ci::gl::ScopedViewport viewportScope(offset, size);
    
    ci::gl::ScopedMatrices matricies;
    ci::gl::setMatrices(camera_);

    // ci::gl::pushModelMatrix();
    {
      ci::gl::ScopedGlslProg shader(shader_);
    
      shader_->uniform("LightPosition", light_.direction);
      shader_->uniform("LightAmbient",  light_.ambient);
      shader_->uniform("LightDiffuse",  light_.diffuse);

      ci::gl::enableDepth(true);
      ci::gl::enable(GL_CULL_FACE);
      ci::gl::clear(GL_DEPTH_BUFFER_BIT);

      ci::gl::pushModelMatrix();
    
      ci::gl::translate(bg_translate_);
      // ci::gl::rotate(bg_rotate_);
    
      ci::gl::draw(model_[0]);
      ci::gl::popModelMatrix();

      if (new_item_) {
        ci::gl::pushModelMatrix();

        ci::gl::translate(new_translate_);
        ci::gl::draw(model_[1]);

        ci::gl::popModelMatrix();
      }
      
      ci::gl::translate(center_);
      ci::gl::rotate(drag_rotate_);
      ci::gl::translate(offset_);
    
      item_.draw(light_);
    }
    // ci::gl::popModelMatrix();
    
    // ci::gl::enableDepth(false);
    // ci::gl::color(1, 0, 0);
    // ci::gl::drawStrokedCube(aabb_);
  }

};

}
