#pragma once

#include <vtkInteractorStyleImage.h>
#include <vtkPropPicker.h>
#include <vtkRenderWindowInteractor.h>

namespace shapeworks {

class Lightbox;

// Handle mouse events
class StudioSliceInteractorStyle : public vtkInteractorStyleImage {
 public:
  static StudioSliceInteractorStyle* New();
  vtkTypeMacro(StudioSliceInteractorStyle, vtkInteractorStyleImage);

  void set_lightbox(Lightbox* lightbox);

  StudioSliceInteractorStyle();

  virtual ~StudioSliceInteractorStyle();

  void OnLeftButtonDown() override;
  void OnRightButtonDown() override;
  void OnMouseWheelForward() override;
  void OnMouseWheelBackward() override;
  void OnKeyDown() override;
  void WindowLevel() override;

 protected:
 private:
  Lightbox* lightbox_;
};

}  // namespace shapeworks
