/*
 * Shapeworks license
 */

/**
 * @file MeshManager.h
 * @brief Class to manage meshes
 *
 * The MeshManager handles all aspects of mesh generation and caching.
 * It houses the cache and manages threads to work on mesh generation
 * in the background.
 */

#ifndef MESH_MANAGER_H
#define MESH_MANAGER_H

#include <vtkSmartPointer.h>

#include <QWaitCondition>

#include <MeshCache.h>
#include <MeshGenerator.h>
#include <MeshWorkQueue.h>
#include <MeshWorker.h>
#include <Preferences.h>

class MeshManager : public QObject
{
  Q_OBJECT

public:
  MeshManager(Preferences& prefs);
  ~MeshManager();

  // pre-generate and cache a mesh for this shape
  void generateMesh( const vnl_vector<double>& shape );

  vtkSmartPointer<vtkPolyData> getMesh( const vnl_vector<double>& shape );

private Q_SLOTS:
  void initializeThreads();

private:

  void shutdownThreads();
  
  Preferences& prefs_;

  // cache of shape meshes
  MeshCache meshCache;

  // our own mesh generator
  MeshGenerator meshGenerator;

  // queue of meshes to build
  MeshWorkQueue workQueue;

  // list of meshes that are currently being worked on in other threads
  MeshWorkQueue workingQueue;

  // the workers
  std::vector<QThread*> threads;
  std::vector<MeshWorker*> workers;

  // condition to wake us up when work is completed by one of the workers
  // used when we are asked for a mesh that is currently being worked on by another thread
  QWaitCondition workDoneCondition;
};

#endif // ifndef MESH_Manager_H
