import pyvista as pv
import numpy as np
import os

# converts shapeworks Image object to vtk image
def sw2vtkImage(swImg, verbose = False):

    # get the numpy array of the shapeworks image as if it were in fortran order
    array  = swImg.toArray()

    # converting a numpy array to a vtk image using pyvista's wrap function
    vtkImg = pv.wrap(array)

    # set spacing
    spacing = swImg.spacing()
    vtkImg.spacing = [spacing[0], spacing[1], spacing[2]]

    # set origin
    origin = swImg.origin()
    vtkImg.origin = [origin[0], origin[1], origin[2]]

    if verbose:
        print('shapeworks image header information: ')
        print(swImg)

        print('\nvtk image header information: ')
        print(vtkImg)

    return vtkImg

# converts shapeworks Mesh object to vtk mesh
def sw2vtkMesh(swMesh, verbose = False):

    # get points and faces of shapeworks mesh
    points = swMesh.points()
    faces = swMesh.faces()

    # create polydata
    vtkMesh = pv.PolyData(points, faces)

    if verbose:
        print('shapeworks mesh header information: ')
        print(swMesh)

        print('vtk mesh header information: ')
        print(vtkMesh)
    
    return vtkMesh
