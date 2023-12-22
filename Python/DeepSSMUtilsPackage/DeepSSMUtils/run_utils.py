import random
import math
import os
import numpy as np

import shapeworks as sw
from bokeh.util.terminal import trace
from shapeworks.utils import sw_message
from shapeworks.utils import sw_progress
from shapeworks.utils import sw_check_abort

import DataAugmentationUtils
import DeepSSMUtils


def create_split(project, train, val, test):
    # Create split

    subjects = project.get_subjects()

    print(f"Creating split: {len(subjects)} subjects")

    # create an array of all the index numbers from 0 to the number of subjects
    subject_indices = list(range(len(subjects)))

    # set the random seed
    random.seed(972)

    # shuffle indices
    random.shuffle(subject_indices)

    # get the number of subjects in each split
    test_val_size = int(math.ceil(len(subject_indices) * .10))
    test_indices = sorted(subject_indices[:test_val_size])
    val_indices = sorted(subject_indices[test_val_size: test_val_size * 2])
    train_indices = sorted(subject_indices[test_val_size * 2:])

    sw_message(f"Creating split: train:{train}%, val:{val}%, test:{test}%")
    sw_message(f"Split sizes: train:{len(train_indices)}, val:{len(val_indices)}, test:{len(test_indices)}")

    for i in range(len(subjects)):
        extra_values = subjects[i].get_extra_values()
        if i in test_indices:
            extra_values["split"] = "test"
        elif i in val_indices:
            extra_values["split"] = "val"
        else:
            extra_values["split"] = "train"
        subjects[i].set_extra_values(extra_values)

    project.set_subjects(subjects)


def groom_training_shapes(project):
    """ Prepare the data for grooming the training data only. """
    subjects = project.get_subjects()

    # Exclude all except training
    for i in range(len(subjects)):
        extra_values = subjects[i].get_extra_values()
        subjects[i].set_excluded(extra_values["split"] != "train")

    project.set_subjects(subjects)

    groom = sw.Groom(project)
    groom.run()


def groom_validation_shapes(project):
    """ Prepare the data for grooming the training data only. """
    subjects = project.get_subjects()
    prep_project_for_val_particles(project)
    groom = sw.Groom(project)
    groom.run()


def prep_project_for_val_particles(project):
    """ Prepare the project for grooming the validation particles. """
    subjects = project.get_subjects()

    # Exclude all except validation
    for i in range(len(subjects)):
        extra_values = subjects[i].get_extra_values()
        subjects[i].set_excluded(extra_values["split"] == "test")
        subjects[i].set_fixed(extra_values["split"] == "train")

    project.set_subjects(subjects)


def get_reference_index(project):
    """ Get the index of the reference subject chosen by grooming alignment."""
    params = project.get_parameters("groom")
    reference_index = params.get("alignment_reference_chosen")
    return int(reference_index)


def get_image_filename(subject):
    """ Get the image filename for a subject. """
    image_map = subject.get_feature_filenames()
    # get the first image
    image_name = list(image_map.values())[0]
    return image_name


def get_deepssm_dir(project):
    """ Get the directory for deepssm data"""
    project_path = project.get_project_path()
    print(f"Project path: {project_path}")
    deepssm_dir = project_path + "/deepssm/"
    # if dir project_path/deepssm doesn't exist, create it using python
    if not os.path.exists(deepssm_dir):
        os.makedirs(deepssm_dir)
    return deepssm_dir


def get_split_indices(project, split):
    """ Get the indices of the subjects. """
    subjects = project.get_subjects()
    training_indices = []
    for i in range(len(subjects)):
        extra_values = subjects[i].get_extra_values()
        is_training = extra_values["split"] == split
        if is_training:
            training_indices.append(i)
    return training_indices


def get_training_indices(project):
    """ Get the indices of the training subjects. """
    return get_split_indices(project, "train")


def get_training_bounding_box(project):
    """ Get the bounding box of the training subjects. """
    subjects = project.get_subjects()
    training_indices = get_training_indices(project)
    training_bounding_box = None
    train_mesh_list = []
    for i in training_indices:
        subject = subjects[i]
        mesh = sw.utils.load_mesh(subject.get_groomed_filenames()[0])
        # apply transform
        alignment = convert_transform_to_numpy(subject.get_groomed_transforms()[0])
        mesh.applyTransform(alignment)
        train_mesh_list.append(mesh)

    bounding_box = sw.MeshUtils.boundingBox(train_mesh_list).pad(10)
    return bounding_box


def convert_transform_to_numpy(transform):
    transform = np.array(transform)
    transform = transform.reshape(4, 4)
    return transform


def groom_training_images(project):
    """ Groom the training images """
    subjects = project.get_subjects()

    deepssm_dir = get_deepssm_dir(project)

    ref_index = get_reference_index(project)
    ref_image = sw.Image(get_image_filename(subjects[ref_index]))
    ref_mesh = sw.utils.load_mesh(subjects[ref_index].get_groomed_filenames()[0])
    # apply transform
    ref_translate = ref_mesh.center()
    ref_image.setOrigin(ref_image.origin() - ref_translate)
    ref_mesh.translate(-ref_translate)

    # check if this subject needs reflection
    needs_reflection, axis = does_subject_need_reflection(project, subjects[ref_index])

    # apply reflection
    reflection = np.eye(4)
    if needs_reflection:
        reflection[axis, axis] = -1
    ref_image.applyTransform(reflection)

    ref_image.write(deepssm_dir + "reference_image.nrrd")

    # determine bounding box of meshes
    bounding_box = get_training_bounding_box(project)

    bounding_box_string = bounding_box.to_string()
    bounding_box_file = deepssm_dir + "bounding_box.txt"
    with open(bounding_box_file, "w") as f:
        f.write(bounding_box_string)

    sw_message("Grooming training images")
    for i in get_training_indices(project):
        image_name = get_image_filename(subjects[i])
        print(f"Grooming {image_name}")
        image = sw.Image(image_name)
        subject = subjects[i]
        # get alignment transform
        alignment = convert_transform_to_numpy(subject.get_groomed_transforms()[0])
        # get procrustes transform
        procrustes = convert_transform_to_numpy(subject.get_procrustes_transforms()[0])

        combined_transform = np.matmul(procrustes, alignment)

        # apply combined transform
        image.applyTransform(combined_transform, ref_image.origin(), ref_image.dims(),
                             ref_image.spacing(), ref_image.coordsys(),
                             sw.InterpolationType.Linear, meshTransform=True)

        # apply bounding box
        image.crop(bounding_box)

        # write image using the index of the subject
        image.write(deepssm_dir + f"/train_images/{i}.nrrd")


def run_data_augmentation(project, num_samples, num_dim, percent_variability, sampler, mixture_num=0, processes=1):
    """ Run data augmentation on the training images. """
    deepssm_dir = get_deepssm_dir(project)
    aug_dir = deepssm_dir + "augmentation/"

    subjects = project.get_subjects()

    # Gather the training image filenames and training world particle filenames
    train_image_filenames = []
    train_world_particle_filenames = []
    for i in get_training_indices(project):
        train_image_filenames.append(deepssm_dir + f"/train_images/{i}.nrrd")
        particle_file = subjects[i].get_world_particle_filenames()[0]
        train_world_particle_filenames.append(particle_file)

    embedded_dim = DataAugmentationUtils.runDataAugmentation(aug_dir, train_image_filenames,
                                                             train_world_particle_filenames, num_samples,
                                                             num_dim, percent_variability,
                                                             sampler, mixture_num, processes)

    # store embedded_dim in file
    with open(deepssm_dir + "embedded_dim.txt", "w") as f:
        f.write(str(embedded_dim))

    # print("Dimensions retained: " + str(embedded_dim))
    return embedded_dim
    # aug_data_csv = aug_dir + "TotalData.csv"


def does_subject_need_reflection(project, subject):
    """ Determine if the subject needs to be reflected.  Return True/False and the Axis """

    params = project.get_parameters("groom")
    do_reflect = params.get("reflect") == "1" or params.get("reflect") == "True"
    if not do_reflect:
        return False, None

    reflect_column = params.get("reflect_column")
    reflect_choice = params.get("reflect_choice")
    reflect_axis = params.get("reflect_axis")
    if reflect_axis == "X":
        reflect_axis = 0
    elif reflect_axis == "Y":
        reflect_axis = 1
    elif reflect_axis == "Z":
        reflect_axis = 2

    extra_values = subject.get_extra_values()
    if extra_values[reflect_column] == reflect_choice:
        return True, reflect_axis
    else:
        return False, None


def transform_to_string(transform):
    """ Convert a transform to a string of numbers with spaces """

    # first convert to list
    transform_list = transform.flatten().tolist()

    # convert to string using join with spaces
    transform_string = " ".join([str(item) for item in transform_list])

    return transform_string


def groom_val_test_images(project):
    """ Groom the validation and test images """
    subjects = project.get_subjects()
    deepssm_dir = get_deepssm_dir(project)

    # Get reference image
    ref_image_file = deepssm_dir + 'reference_image.nrrd'
    ref_image = sw.Image(ref_image_file)
    ref_center = ref_image.center()  # get center

    # Read bounding box
    bounding_box_file = deepssm_dir + "bounding_box.txt"
    with open(bounding_box_file, "r") as f:
        bounding_box_string = f.read()
    bounding_box = sw.PhysicalRegion(bounding_box_string)

    # Slightly cropped ref image
    large_bb = sw.PhysicalRegion(bounding_box.min, bounding_box.max).pad(80)
    large_cropped_ref_image_file = deepssm_dir + 'large_cropped_reference_image.nrrd'
    large_cropped_ref_image = sw.Image(ref_image_file).crop(large_bb).write(large_cropped_ref_image_file)
    # Further cropped ref image
    medium_bb = sw.PhysicalRegion(bounding_box.min, bounding_box.max).pad(20)
    medium_cropped_ref_image_file = deepssm_dir + 'medium_cropped_reference_image.nrrd'
    medium_cropped_ref_image = sw.Image(ref_image_file).crop(medium_bb).write(medium_cropped_ref_image_file)
    # Fully cropped ref image
    cropped_ref_image_file = deepssm_dir + 'cropped_reference_image.nrrd'
    cropped_ref_image = sw.Image(ref_image_file).crop(bounding_box).write(cropped_ref_image_file)

    # Make dirs
    val_test_images_dir = deepssm_dir + 'val_and_test_images/'
    if not os.path.exists(val_test_images_dir):
        os.makedirs(val_test_images_dir)

    val_indices = get_split_indices(project, "val")
    test_indices = get_split_indices(project, "test")

    val_test_indices = val_indices + test_indices

    val_test_transforms = []
    val_test_image_files = []

    for i in val_test_indices:
        image_name = get_image_filename(subjects[i])
        sw_message(f"loading {image_name}")
        print(f"pwd = {os.getcwd()}")
        image = sw.Image(image_name)

        image_file = val_test_images_dir + f"{i}.nrrd"

        # check if this subject needs reflection
        needs_reflection, axis = does_subject_need_reflection(project, subjects[i])

        # apply reflection
        reflection = np.eye(4)
        if needs_reflection:
            reflection[axis, axis] = -1
        image.applyTransform(reflection)
        transform = sw.utils.getVTKtransform(reflection)

        # 2. Translate to have ref center to make rigid registration easier
        translation = ref_center - image.center()
        image.setOrigin(image.origin() + translation).write(image_file)
        transform[:3, -1] += translation

        # 3. Translate with respect to slightly cropped ref
        image = sw.Image(image_file).crop(large_bb).write(image_file)
        itk_translation_transform = DeepSSMUtils.get_image_registration_transform(large_cropped_ref_image_file,
                                                                                  image_file,
                                                                                  transform_type='translation')
        # Apply transform
        image.applyTransform(itk_translation_transform,
                             large_cropped_ref_image.origin(), large_cropped_ref_image.dims(),
                             large_cropped_ref_image.spacing(), large_cropped_ref_image.coordsys(),
                             sw.InterpolationType.Linear, meshTransform=False)
        vtk_translation_transform = sw.utils.getVTKtransform(itk_translation_transform)
        transform = np.matmul(vtk_translation_transform, transform)

        # 4. Crop with medium bounding box and find rigid transform
        image.crop(medium_bb).write(image_file)
        itk_rigid_transform = DeepSSMUtils.get_image_registration_transform(medium_cropped_ref_image_file,
                                                                            image_file, transform_type='rigid')
        # Apply transform
        image.applyTransform(itk_rigid_transform,
                             medium_cropped_ref_image.origin(), medium_cropped_ref_image.dims(),
                             medium_cropped_ref_image.spacing(), medium_cropped_ref_image.coordsys(),
                             sw.InterpolationType.Linear, meshTransform=False)
        vtk_rigid_transform = sw.utils.getVTKtransform(itk_rigid_transform)
        transform = np.matmul(vtk_rigid_transform, transform)

        # 5. Get similarity transform from image registration and apply
        image.crop(bounding_box).write(image_file)
        itk_similarity_transform = DeepSSMUtils.get_image_registration_transform(cropped_ref_image_file,
                                                                                 image_file,
                                                                                 transform_type='similarity')
        image.applyTransform(itk_similarity_transform,
                             cropped_ref_image.origin(), cropped_ref_image.dims(),
                             cropped_ref_image.spacing(), cropped_ref_image.coordsys(),
                             sw.InterpolationType.Linear, meshTransform=False)
        vtk_similarity_transform = sw.utils.getVTKtransform(itk_similarity_transform)
        transform = np.matmul(vtk_similarity_transform, transform)
        # Save transform
        val_test_transforms.append(transform)
        extra_values = subjects[i].get_extra_values()
        extra_values["registration_transform"] = transform_to_string(transform)

        subjects[i].set_extra_values(extra_values)
    project.set_subjects(subjects)


def prepare_data_loaders(project, batch_size):
    """ Prepare PyTorch laoders """
    val_indicies = get_split_indices(project, "val")

    deepssm_dir = get_deepssm_dir(project)
    aug_dir = deepssm_dir + "augmentation/"
    aug_data_csv = aug_dir + "TotalData.csv"

    loader_dir = deepssm_dir + 'torch_loaders/'

    val_image_files = []
    val_world_particles = []
    for i in val_indicies:
        val_image_files.append(deepssm_dir + f"/val_and_test_images/{i}.nrrd")
        particle_file = project.get_subjects()[i].get_world_particle_filenames()[0]
        val_world_particles.append(particle_file)

    test_image_files = []
    test_indicies = get_split_indices(project, "test")
    for i in test_indicies:
        test_image_files.append(deepssm_dir + f"/val_and_test_images/{i}.nrrd")

    DeepSSMUtils.getTrainLoader(loader_dir, aug_data_csv, batch_size)
    DeepSSMUtils.getValidationLoader(loader_dir, val_image_files, val_world_particles)
    DeepSSMUtils.getTestLoader(loader_dir, test_image_files)
