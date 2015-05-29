/*
 * Shapeworks license
 */

#include <Data/MeshGenerator.h>

// vtk
#include <vtkContourFilter.h>
#include <vtkPointData.h>
#include <vtkPolyDataNormals.h>
#include <vtkReverseSense.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkUnsignedLongArray.h>
#include <vtkTriangleFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkDecimatePro.h>
#include <vtkButterflySubdivisionFilter.h>
#include <Visualization/vtkPolyDataToImageData.h>

#include <vtkMetaImageWriter.h>

#include <Visualization/CustomSurfaceReconstructionFilter.h>

// local files
#ifdef POWERCRUST
#include <Visualization/vtkPowerCrustSurfaceReconstruction.h>
#endif

MeshGenerator::MeshGenerator(Preferences& prefs) : prefs_(prefs)
{
  this->points = vtkSmartPointer<vtkPoints>::New();
  this->points->SetDataTypeToDouble();
  this->pointSet = vtkSmartPointer<vtkPolyData>::New();
  this->pointSet->SetPoints( this->points );

  this->surfaceReconstruction = vtkSmartPointer<CustomSurfaceReconstructionFilter>::New();
#if VTK_MAJOR_VERSION <= 5
  this->surfaceReconstruction->SetInput(this->pointSet);
#else
  this->surfaceReconstruction->SetInputData(this->pointSet);
#endif

#ifdef POWERCRUST
  this->polydataToImageData = vtkSmartPointer<vtkPolyDataToImageData>::New();
  this->triangleFilter = vtkSmartPointer<vtkTriangleFilter>::New();
  this->powercrust = vtkSmartPointer<vtkPowerCrustSurfaceReconstruction>::New();
#if VTK_MAJOR_VERSION <= 5
  this->powercrust->SetInput(this->pointSet);
#else
  this->powercrust->SetInputData(this->pointSet);
#endif
  this->triangleFilter->SetInputConnection( this->powercrust->GetOutputPort() );
  this->polydataToImageData->SetInputConnection( this->triangleFilter->GetOutputPort() );
#endif // ifdef POWERCRUST

  this->contourFilter = vtkSmartPointer<vtkContourFilter>::New();
  this->contourFilter->SetInputConnection( this->surfaceReconstruction->GetOutputPort() );
  this->contourFilter->SetValue( 0, 0.0 );
  this->contourFilter->ComputeNormalsOn();

  this->windowSincFilter = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
  this->windowSincFilter->SetInputConnection( this->contourFilter->GetOutputPort() );

  this->smoothFilter = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
  this->smoothFilter->SetInputConnection( this->contourFilter->GetOutputPort() );

  this->polydataNormals = vtkSmartPointer<vtkPolyDataNormals>::New();
  this->polydataNormals->SplittingOff();

  this->updatePipeline();
}

MeshGenerator::~MeshGenerator()
{}

vtkSmartPointer<vtkPolyData> MeshGenerator::buildMesh( const vnl_vector<double>& shape )
{
  // copy shape points into point set
  int numPoints = shape.size() / 3;
  this->points->SetNumberOfPoints( numPoints );
  unsigned int k = 0;
  for ( unsigned int i = 0; i < numPoints; i++ )
  {
    double x = shape[k++];
    double y = shape[k++];
    double z = shape[k++];
    this->points->SetPoint( i, x, y, z );
  }
  this->points->Modified();
  if ( this->prefs_.get_use_powercrust() )
  {
    if (this->prefs_.get_smoothing_amount() > 0)
    {
      this->polydataToImageData->Update();
      this->contourFilter->Update();
    }
  }
  else
  {
    this->surfaceReconstruction->Modified();
    this->surfaceReconstruction->Update();
    this->contourFilter->Update();
  }
  this->polydataNormals->Update();

  // make a copy of the vtkPolyData output to return
  vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
  polyData->DeepCopy( this->polydataNormals->GetOutput() );

  return polyData;
}

void MeshGenerator::updatePipeline()
{
  if ( this->prefs_.get_use_powercrust() && this->prefs_.get_smoothing_amount() > 0 )
  {
    this->windowSincFilter->SetNumberOfIterations( this->prefs_.get_smoothing_amount() );
    this->windowSincFilter->SetPassBand( 0.05 );
    this->contourFilter->SetInputConnection( this->polydataToImageData->GetOutputPort() );
    this->contourFilter->SetValue( 0, 0.5 );
    this->polydataNormals->SetInputConnection( this->windowSincFilter->GetOutputPort() );
  }
  else if ( this->prefs_.get_use_powercrust() && !(this->prefs_.get_smoothing_amount() > 0) )
  {
#ifdef POWERCRUST
    this->polydataNormals->SetInputConnection( this->powercrust->GetOutputPort() );
#endif
  }
  else if ( !this->prefs_.get_use_powercrust() && this->prefs_.get_smoothing_amount() > 0 )
  {
	this->surfaceReconstruction->SetNeighborhoodSize( this->prefs_.get_neighborhood() );
    this->surfaceReconstruction->SetSampleSpacing( this->prefs_.get_spacing()  );
    this->windowSincFilter->SetNumberOfIterations( this->prefs_.get_smoothing_amount() );
    this->windowSincFilter->SetPassBand( 0.05 );
    this->contourFilter->SetInputConnection( this->surfaceReconstruction->GetOutputPort() );
    this->contourFilter->SetValue( 0, 0.0 );
    this->polydataNormals->SetInputConnection( this->windowSincFilter->GetOutputPort() );
  }
  else if ( !this->prefs_.get_use_powercrust() && !( this->prefs_.get_smoothing_amount() > 0) )
  {
	this->surfaceReconstruction->SetNeighborhoodSize( this->prefs_.get_neighborhood() );
    this->surfaceReconstruction->SetSampleSpacing( this->prefs_.get_spacing()  );
    this->polydataNormals->SetInputConnection( this->contourFilter->GetOutputPort() );
  }
}
