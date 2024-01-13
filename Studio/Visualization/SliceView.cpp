// vtk
#include <vtkCamera.h>
#include <vtkCellData.h>
#include <vtkCutter.h>
#include <vtkImageActor.h>
#include <vtkImageActorPointPlacer.h>
#include <vtkImageProperty.h>
#include <vtkImageSliceMapper.h>
#include <vtkNamedColors.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkStripper.h>
#include <vtkTransformPolyDataFilter.h>

// shapeworks
#include <Visualization/SliceView.h>
#include <Visualization/Viewer.h>

namespace shapeworks {

//-----------------------------------------------------------------------------
vtkSmartPointer<vtkActor> SliceView::create_shape_actor(vtkSmartPointer<vtkPolyData> poly_data, QColor color) {
  auto cut_transform_filter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  auto cutter = vtkSmartPointer<vtkCutter>::New();
  auto stripper = vtkSmartPointer<vtkStripper>::New();
  auto cut_mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  auto cut_actor = vtkSmartPointer<vtkActor>::New();

  stripper->PassCellDataAsFieldDataOn();
  cut_mapper->SetScalarVisibility(false);
  cut_mapper->SetResolveCoincidentTopologyToPolygonOffset();
  cut_actor->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
  cut_actor->GetProperty()->SetLineWidth(4);
  cut_actor->GetProperty()->SetAmbient(1.0);
  cut_actor->GetProperty()->SetDiffuse(0.0);

  auto transform = viewer_->get_image_transform();
  cut_transform_filter->SetInputData(poly_data);
  cut_transform_filter->SetTransform(transform);

  cutter->SetInputConnection(cut_transform_filter->GetOutputPort());
  cutter->SetCutFunction(slice_mapper_->GetSlicePlane());

  stripper->SetInputConnection(cutter->GetOutputPort());
  stripper->Update();

  cut_mapper->SetInputConnection(stripper->GetOutputPort());

  cut_actor->SetMapper(cut_mapper);

  return cut_actor;
}

//-----------------------------------------------------------------------------
SliceView::SliceView(Viewer *viewer) : viewer_(viewer) {
  image_slice_ = vtkSmartPointer<vtkImageActor>::New();

  slice_mapper_ = vtkSmartPointer<vtkImageSliceMapper>::New();

  placer_ = vtkSmartPointer<vtkImageActorPointPlacer>::New();
  placer_->SetImageActor(image_slice_);
}

//-----------------------------------------------------------------------------
void SliceView::set_volume(std::shared_ptr<Image> volume) {
  volume_ = volume;
  if (!volume) {
    update_renderer();
    return;
  }
  vtk_volume_ = volume->getVTKImage();
  slice_mapper_->SetInputData(vtk_volume_);

  image_slice_->SetMapper(slice_mapper_);

  auto transform = viewer_->get_image_transform();
  image_slice_->SetUserTransform(transform);

  update_extent();
}

//-----------------------------------------------------------------------------
void SliceView::add_mesh(vtkSmartPointer<vtkPolyData> poly_data) {
  for (auto &poly : poly_datas_) {
    if (poly == poly_data) {
      return;
    }
  }
  if (poly_data->GetNumberOfCells() == 0) {
    return;
  }
  if (!is_image_loaded()) {
    return;
  }

  poly_datas_.push_back(poly_data);

  static QColor colors[10] = {QColor(255, 0, 0),     QColor(0, 0, 255),   QColor(0, 255, 0),     QColor(255, 255, 0),
                              QColor(255, 0, 255),   QColor(0, 255, 255), QColor(255, 255, 255), QColor(0, 0, 0),
                              QColor(128, 128, 128), QColor(128, 0, 0)};

  int color = cut_actors_.size() % 10;

  auto actor = create_shape_actor(poly_data, colors[color]);
  cut_actors_.push_back(actor);
  viewer_->get_renderer()->AddActor(actor);
}

//-----------------------------------------------------------------------------
void SliceView::clear_meshes() {
  for (auto &actor : cut_actors_) {
    viewer_->get_renderer()->RemoveViewProp(actor);
  }
  cut_actors_.clear();
  poly_datas_.clear();
}

//-----------------------------------------------------------------------------
void SliceView::set_orientation(int orientation) {
  if (orientation == slice_mapper_->GetOrientation()) {
    return;
  }
  slice_mapper_->SetOrientation(orientation);

  update_camera();
}

//-----------------------------------------------------------------------------
bool SliceView::is_image_loaded() { return volume_ != nullptr; }

//-----------------------------------------------------------------------------
void SliceView::update_renderer() {
  auto renderer = viewer_->get_renderer();

  if (is_image_loaded()) {
    renderer->AddActor(image_slice_);
    for (auto &actor : cut_actors_) {
      renderer->AddActor(actor);
    }
  } else {
    renderer->RemoveViewProp(image_slice_);
    for (auto &actor : cut_actors_) {
      renderer->RemoveViewProp(actor);
    }
  }
}

//-----------------------------------------------------------------------------
void SliceView::update_camera() {
  auto renderer = viewer_->get_renderer();
  if (!is_image_loaded()) {
    renderer->GetActiveCamera()->SetParallelProjection(0);
    return;
  }

  int orientation = slice_mapper_->GetOrientation();
  double origin[3];
  int dims[3];
  double spacing[3];
  vtk_volume_->GetOrigin(origin);
  vtk_volume_->GetDimensions(dims);
  vtk_volume_->GetSpacing(spacing);

  int max_slice_num = slice_mapper_->GetSliceNumberMinValue();
  // start in the middle
  current_slice_number_ = (slice_mapper_->GetSliceNumberMaxValue() - slice_mapper_->GetSliceNumberMinValue()) / 2;
  slice_mapper_->SetSliceNumber(current_slice_number_);

  if (orientation == 0) {
    renderer->GetActiveCamera()->SetPosition(spacing[0] * (max_slice_num + 1), spacing[1] * dims[1] / 2,
                                             spacing[2] * dims[2] / 2);
    renderer->GetActiveCamera()->SetViewUp(0, 0, 1);
    renderer->GetActiveCamera()->SetFocalPoint(0, spacing[1] * dims[1] / 2, spacing[2] * dims[2] / 2);
    renderer->GetActiveCamera()->SetParallelScale(spacing[2] * dims[2]);
  } else if (orientation == 1) {
    renderer->GetActiveCamera()->SetPosition(spacing[0] * dims[0] / 2, spacing[1] * (max_slice_num + 1),
                                             spacing[2] * dims[2] / 2);
    renderer->GetActiveCamera()->SetViewUp(0, 0, 1);
    renderer->GetActiveCamera()->SetFocalPoint(spacing[0] * dims[0] / 2, 0, spacing[2] * dims[2] / 2);
    renderer->GetActiveCamera()->SetParallelScale(spacing[2] * dims[2]);
  } else {
    renderer->GetActiveCamera()->SetPosition(spacing[0] * dims[0] / 2, spacing[1] * dims[1] / 2,
                                             spacing[2] * (max_slice_num + 1));
    renderer->GetActiveCamera()->SetViewUp(0, 1, 0);
    renderer->GetActiveCamera()->SetFocalPoint(spacing[0] * dims[0] / 2, spacing[1] * dims[1] / 2, 0);
    renderer->GetActiveCamera()->SetParallelScale(spacing[1] * dims[1]);
  }

  renderer->GetActiveCamera()->SetParallelProjection(1);
  renderer->ResetCameraClippingRange();
  renderer->ResetCamera();
  update_extent();
}

//-----------------------------------------------------------------------------
Point SliceView::handle_key(std::string key) {
  if (!is_image_loaded()) {
    return Point({0, 0, 0});
  }

  if (key == "Up") {
    set_slice_number(current_slice_number_ + 1);
  } else if (key == "Down") {
    set_slice_number(current_slice_number_ - 1);
  }

  auto plane = slice_mapper_->GetSlicePlane();
  auto origin = plane->GetOrigin();
  return Point({origin[0], origin[1], origin[2]});
}

//-----------------------------------------------------------------------------
void SliceView::change_slice(SliceChange change) {
  if (!is_image_loaded()) {
    return;
  }

  if (change == SliceChange::Up) {
    set_slice_number(current_slice_number_ + 1);
  } else if (change == SliceChange::Down) {
    set_slice_number(current_slice_number_ - 1);
  }
}

//-----------------------------------------------------------------------------
Point SliceView::get_slice_position() {
  if (!is_image_loaded()) {
    return Point({0, 0, 0});
  }

  double origin[3];
  vtk_volume_->GetOrigin(origin);
  double spacing[3];
  vtk_volume_->GetSpacing(spacing);

  Point result = origin;

  int i = get_orientation_index();
  result[i] = origin[i] + spacing[i] * current_slice_number_;
  return result;
}

//-----------------------------------------------------------------------------
void SliceView::set_slice_position(Point point) {
  if (!is_image_loaded()) {
    return;
  }
  auto index = volume_->getITKImage()->TransformPhysicalPointToIndex(point);
  int slice_number = index[get_orientation_index()];
  set_slice_number(slice_number);
}

//-----------------------------------------------------------------------------
void SliceView::set_window_and_level(double window, double level) {
  image_slice_->GetProperty()->SetColorWindow(window);
  image_slice_->GetProperty()->SetColorLevel(level);
}

//-----------------------------------------------------------------------------
double SliceView::get_spacing() {
  if (!volume_) {
    return 1.0;
  }
  int index = get_orientation_index();
  double *spacing = vtk_volume_->GetSpacing();
  return spacing[index];
}

//-----------------------------------------------------------------------------
int SliceView::get_orientation_index() { return slice_mapper_->GetOrientation(); }

//-----------------------------------------------------------------------------
void SliceView::update_particles() {}

//-----------------------------------------------------------------------------
Viewer *SliceView::get_viewer() { return viewer_; }

//-----------------------------------------------------------------------------
bool SliceView::should_point_show(double x, double y, double z) {
  if (!is_image_loaded()) {
    return true;
  }
  vtkSmartPointer<vtkPlane> plane = vtkSmartPointer<vtkPlane>::New();

  double bounds[6];

  slice_mapper_->GetBounds(bounds);

  int orientation = this->get_orientation_index();

  if (orientation == 2) {
    plane->SetNormal(0, 0, 1);
    plane->SetOrigin(0, 0, bounds[4]);
  } else if (orientation == 1) {
    plane->SetNormal(0, 1, 0);
    plane->SetOrigin(0, bounds[2], 0);
  } else {
    plane->SetNormal(1, 0, 0);
    plane->SetOrigin(bounds[0], 0, 0);
  }

  double point[3];
  point[0] = x;
  point[1] = y;
  point[2] = z;
  double distance = plane->DistanceToPlane(point);
  double spacing = get_spacing();
  int slice_number = (distance + (spacing / 2.0)) / spacing;
  if (slice_number == current_slice_number_) {
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------
void SliceView::set_slice_number(int slice) {
  current_slice_number_ = slice;

  current_slice_number_ = std::min(current_slice_number_, slice_mapper_->GetSliceNumberMaxValue());
  current_slice_number_ = std::max(current_slice_number_, slice_mapper_->GetSliceNumberMinValue());

  slice_mapper_->SetSliceNumber(current_slice_number_);
  viewer_->get_renderer()->GetRenderWindow()->Render();
  update_extent();
  viewer_->update_points();
}

//-----------------------------------------------------------------------------
void SliceView::update_extent() {
  if (!slice_mapper_ || !volume_) {
    return;
  }
  int orientation = slice_mapper_->GetOrientation();

  int *extent = vtk_volume_->GetExtent();

  int slice = current_slice_number_;
  if (orientation == 2) {
    image_slice_->SetDisplayExtent(extent[0], extent[1], extent[2], extent[3], slice, slice);
  } else if (orientation == 1) {
    image_slice_->SetDisplayExtent(extent[0], extent[1], slice, slice, extent[4], extent[5]);
  } else if (orientation == 0) {
    image_slice_->SetDisplayExtent(slice, slice, extent[2], extent[3], extent[4], extent[5]);
  }

  // Figure out the correct clipping range
  vtkCamera *cam = viewer_->get_renderer()->GetActiveCamera();
  if (cam) {
    double bounds[6];
    image_slice_->GetBounds(bounds);

    int index = get_orientation_index();
    double spos = bounds[index * 2];
    double cpos = cam->GetPosition()[index];

    double range = fabs(spos - cpos);

    double spacing = this->get_spacing();

    double near_clip = range - spacing / 2.0 + 0.001;
    double far_clip = range + spacing / 2.0 - 0.001;

    // cam->SetClippingRange( near_clip, far_clip );
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

}  // namespace shapeworks
