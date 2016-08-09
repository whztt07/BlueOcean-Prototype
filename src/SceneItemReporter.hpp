﻿#pragma once

//
// アイテム発見画面
//

#include "SceneBase.hpp"
#include "ItemReporter.hpp"


namespace ngs {

class SceneItemReporter
  : public SceneBase {

  // FIXME:スマートポインタの方が安全
  Event<Arguments>& event_;

  ci::gl::FboRef fbo_;
  ci::gl::GlslProgRef shader_;
  ci::gl::VboMeshRef  model_;
  
  ItemReporter item_reporter_;

  bool active_ = true;

  

  // TIPS:SceneBase* 経由でないと呼び出せなくしたいので
  //      わざとprivate
  bool isActive() const override {
    return active_;
  }
  
  void resize(const float aspect) override {
    item_reporter_.resize(aspect);
  }

  void touchesBegan(const int touching_num, const std::vector<Touch>& touches) override {
    item_reporter_.touchesBegan(touching_num, touches);
  }
  
  void touchesMoved(const int touching_num, const std::vector<Touch>& touches) override {
    item_reporter_.touchesMoved(touching_num, touches);
  }
  
  void touchesEnded(const int touching_num, const std::vector<Touch>& touches) override {
    item_reporter_.touchesEnded(touching_num, touches);
  }

  void update() override {
    item_reporter_.update();
  }
  
  void draw(const bool offscreen) override {
    {
      ci::gl::ScopedGlslProg shader(shader_);
      ci::gl::ScopedTextureBind texture(fbo_->getColorTexture());

      ci::gl::enableDepth(false);
      ci::gl::disable(GL_CULL_FACE);
      ci::gl::disableAlphaBlending();

      ci::gl::draw(model_);
    }
    
    item_reporter_.draw();
  }

  
public:
  SceneItemReporter(Event<Arguments>& event,
                    const ci::JsonTree& params,
                    const ci::gl::FboRef& fbo,
                    const int index)
    : event_(event),
      fbo_(fbo),
      item_reporter_(event, params["item_reporter"])
  {
    item_reporter_.loadItem(params["item.body"][index]);

    shader_ = createShader("bg", "bg");
    model_  = ci::gl::VboMesh::create(ci::ObjLoader(Asset::load("bg.obj")));
    
    event_.connect("close_item_reporter",
                   [this](const Connection&, const Arguments&) {
                     active_ = false;
                   });
  }

};

}
