#ifndef __RECONSTRUCTION_H__
#define __RECONSTRUCTION_H__

#include "itkCompactlySupportedRBFSparseKernelTransform.h"
#include "ShapeWorksGroom.h"
#include "ITKVTK/itkImageToVTKImageFilter.h"
#include "ITKVTK/itkVTKImageToImageFilter.h"
#include <vtkPolyData.h>
#include <itkAddImageFilter.h>
#include <itkGradientImageFilter.h>
#include <itkGradientMagnitudeImageFilter.h>
#include <itkResampleImageFilter.h>
#include <itkBSplineInterpolateImageFunction.h>
#include <itkMultiplyImageFilter.h>
#include "itkImageRegionConstIterator.h"

#define Dimension 3
class Reconstruction {
  typedef itk::GradientImageFilter<ImageType, float> 
                                                     GradientFilterType;
  typedef itk::GradientMagnitudeImageFilter<ImageType, ImageType >
                                                     GradientMagnitudeFilterType;
  typedef itk::Image< itk::CovariantVector< float, 3 >, 3 > 
                                                     GradientImageType;
  typedef itk::ImageRegionIterator< GradientImageType > 
                                                     GradientImageIteratorType;
  typedef itk::ImageRegionIterator< ImageType >      ImageIteratorType;

  typedef itk::ImageToVTKImageFilter<ImageType>      ITK2VTKConnectorType;
  typedef itk::AddImageFilter <ImageType, ImageType >
                                                     AddImageFilterType;
  typedef itk::ResampleImageFilter<ImageType, ImageType >
                                                     ResampleFilterType;
  typedef itk::BSplineInterpolateImageFunction<ImageType, double, double > 
                                                     InterpolatorType;
  typedef itk::MultiplyImageFilter <ImageType, ImageType, ImageType>
                                                     MultiplyByConstantImageFilterType;
  typedef double                                     CoordinateRepType;
  typedef itk::CompactlySupportedRBFSparseKernelTransform 
    <CoordinateRepType, Dimension>                    RBFTransformType;
  typedef itk::Point< CoordinateRepType, Dimension >  PointType;
  typedef std::vector< PointType >                    PointArrayType;
  typedef RBFTransformType::PointSetType              PointSetType;
  typedef PointSetType::PointIdentifier               PointIdType;
public:
  Reconstruction(
    bool denseDone = false,
    float levelsetValue = 0.f,
    float targetReduction = 0.3,
    float featureAngle = 30.,
    int lsSmootherIterations = 5,
    int meshSmootherIterations = 5,
    bool preserveTopology = true,
    bool fixWinding = true,
    bool smoothBeforeDecimate = true,
    bool smoothAfterDecimate = true,
    float smoothingLambda = 0.5,
    int qcSmoothIterations = 1,
    float decimationPercent = 0.3
    );
  ~Reconstruction();
  vtkSmartPointer<vtkPolyData> getMean(
    std::vector<std::vector<itk::Point<float> > > local_pts =
    std::vector<std::vector<itk::Point<float> > >(),
    std::vector<itk::Point<float> > global_pts = 
    std::vector<itk::Point<float> >(),
    std::vector<ImageType::Pointer> distance_transform = 
    std::vector<ImageType::Pointer>() );
  vtkSmartPointer<vtkPolyData> 
    getMesh(std::vector<itk::Point<float> > local_pts);
private:
  void computeDenseMean(
    std::vector<std::vector<itk::Point<float> > > local_pts,
    std::vector<itk::Point<float> > global_pts,
    std::vector<ImageType::Pointer> distance_transform);
  vnl_matrix<double> computeParticlesNormals(
    vtkSmartPointer< vtkPoints > particles,
    ImageType::Pointer distance_transform);
  void generateWarpedMeshes(RBFTransformType::Pointer transform,
    vtkSmartPointer<vtkPolyData>& outputMesh);
  double computeAverageDistanceToNeighbors(vtkSmartPointer<vtkPoints> points, 
    std::vector<int> particles_indices);
  void CheckMapping(vtkSmartPointer<vtkPoints> sourcePts,
    vtkSmartPointer<vtkPoints> targetPts,
    RBFTransformType::Pointer rbfTransform,
    vtkSmartPointer<vtkPoints> & mappedCorrespondences,
    double & rms, double & rms_wo_mapping, double & maxmDist);
  vtkSmartPointer<vtkPoints> convertToImageCoordinates(
    vtkSmartPointer<vtkPoints> particles, int number_of_particles,
    const itk::Image< float, 3 >::SpacingType& spacing,
    const itk::Image< float, 3 >::PointType& origin);
  vtkSmartPointer<vtkPoints> convertToPhysicalCoordinates(
    vtkSmartPointer<vtkPoints> particles, int number_of_particles,
    const itk::Image< float, 3 >::SpacingType& spacing,
    const itk::Image< float, 3 >::PointType& origin);
  vtkSmartPointer<vtkPolyData> extractIsosurface(
    vtkSmartPointer<vtkImageData> volData);
  vtkSmartPointer<vtkPolyData> Reconstruction::MeshQC(
    vtkSmartPointer<vtkPolyData> meshIn);
  //members.
  vtkSmartPointer<vtkPoints> sparseMean_;
  vtkSmartPointer<vtkPolyData> denseMean_;
  std::vector<bool> goodPoints_;
  bool denseDone_; 
  float levelsetValue_;
  float targetReduction_;
  float featureAngle_;
  int lsSmootherIterations_;
  int meshSmootherIterations_;
  bool preserveTopology_, fixWinding_, smoothBeforeDecimation_,
    smoothAfterDecimation_;
  float smoothingLambda_, decimationPercent_;
  int qcSmoothingIterations_;
};
#endif // !__RECONSTRUCTION_H__
