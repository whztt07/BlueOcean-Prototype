﻿#pragma once

//
// アプリの外枠
//

#include "Event.hpp"
#include "Arguments.hpp"
#include "Params.hpp"
#include "Touch.hpp"
#include "SceneGame.hpp"
#include "SceneItemReporter.hpp"
#include <deque>


namespace ngs {

class Worker {
  // 汎用的なコールバック管理
  Event<Arguments> event_;

  // ゲーム内パラメーター
  ci::JsonTree params_;

  // ゲーム世界
  std::shared_ptr<Game> game_;

  // 現在の画面
  std::deque<std::shared_ptr<SceneBase>> scene_stack_;


  ci::gl::FboRef createSnapshot(const std::shared_ptr<SceneBase>& scene) {
    // FBO
    auto format = ci::gl::Fbo::Format()
      .colorTexture()
      ;
    auto fbo = ci::gl::Fbo::create(256, 256, format);

    ci::gl::ScopedViewport viewportScope(fbo->getSize());
    ci::gl::ScopedFramebuffer fboScope(fbo);
    scene->draw(true);

    return fbo;
  }

  
  // シーンファクトリー
  void setupFactory() {
    event_.connect("scene_game",
                   [this](const Connection&, const Arguments&) {
                     DOUT << "scene_game" << std::endl;

                     scene_stack_.push_front(std::make_shared<SceneGame>(event_, params_, game_));
                   });

    event_.connect("scene_item_reporter",
                   [this](const Connection&, const Arguments& arguments) {
                     DOUT << "scene_item_reporter" << std::endl;

                     // 直前の画面のsnapshot
                     auto fbo = createSnapshot(scene_stack_.front());
                     auto index = boost::any_cast<int>(arguments.at("item"));
                     
                     scene_stack_.push_front(std::make_shared<SceneItemReporter>(event_, params_, fbo, index));
                   });
  }
  

public:
  Worker()
    : params_(Params::load("params.json")),
      game_(std::make_shared<Game>(event_, params_))
  {
    setupFactory();

    // 最初のシーンを生成
    event_.signal("scene_game", Arguments());
  }


  void cleanup() {
    game_->cleanup();
  }

  
  void resize(const float aspect) {
    for (const auto& scene : scene_stack_) {
      if (!scene->isActive()) continue;
      
      scene->resize(aspect);
    }
  }


  // touching_numはタッチ操作中の数(新たに発生したのも含む)
  // touchesは新たに発生したタッチイベント内容
  void touchesBegan(const int touching_num, const std::vector<Touch>& touches) {
    scene_stack_.front()->touchesBegan(touching_num, touches);
  }

  // touching_numはタッチ操作中の数
  void touchesMoved(const int touching_num, const std::vector<Touch>& touches) {
    scene_stack_.front()->touchesMoved(touching_num, touches);
  }

  // touching_numは残りのタッチ操作中の数
  void touchesEnded(const int touching_num, const std::vector<Touch>& touches) {
    scene_stack_.front()->touchesEnded(touching_num, touches);
  }
  

  void update() {
    // 無効な画面を削除
    for (auto it = std::begin(scene_stack_); it != std::end(scene_stack_); ) {
      if (!(*it)->isActive()) {
        it = scene_stack_.erase(it);
      }
      else {
        ++it;
      }
    }

    // 最前列の画面だけ更新
    scene_stack_.front()->update();
  }

  void draw() {
    // 最前列の画面だけ描画
    scene_stack_.front()->draw(false);
  }


  // デバッグ用途
  Event<Arguments>& getEvent() { return event_; }
  const ci::JsonTree& getParams() const { return params_; }

};

}