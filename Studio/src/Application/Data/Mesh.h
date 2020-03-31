#pragma once

#include <QString>
#include <QSharedPointer>

#include <itkeigen/Eigen/Dense>
#include <itkeigen/Eigen/Sparse>

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkImageImport.h>
#include <itkImage.h>

#include <vnl/vnl_vector.h>
#include <Groom/ShapeWorksGroom.h>

class Mesh;
typedef QSharedPointer< Mesh > MeshHandle;
//! Representation of a single mesh.
/*!
 * The Mesh class represents a single mesh generated from an image file.
 * It is responsible for loading the image and generating a mesh from it.
 *
 */
class Mesh
{
public:

  /// Constructor
  Mesh();

  /// Destructor
  ~Mesh();

  /// Create a mesh from an image
  ImageType::Pointer create_from_file(std::string filename, double iso_value);
  void create_from_image(ImageType::Pointer img, double iso_value);

  /// Get the dimensions as a string for display (if loaded from an image)
  QString get_dimension_string();

  /// Get the mesh polydata
  vtkSmartPointer<vtkPolyData> get_poly_data();

  /// Get the center transform
  vnl_vector<double> get_center_transform();

  //! Set the poly data directly
  void set_poly_data(vtkSmartPointer<vtkPolyData> poly_data);

private:

  // metadata
  int dimensions_[3];
  vnl_vector<double> center_transform_;

  // the polydata
  vtkSmartPointer<vtkPolyData> poly_data_;
};
