// must come first
#include <Libs/Optimize/Optimize.h>
// must come first

#include <Data/Shape.h>
#include <Data/ShapeWorksWorker.h>
#include <Data/SurfaceReconstructor.h>
#include <Groom/QGroom.h>
#include <Libs/Optimize/OptimizeParameters.h>
#include <Logging.h>

namespace shapeworks {

//---------------------------------------------------------------------------
ShapeworksWorker::ShapeworksWorker(ThreadType type, QSharedPointer<QGroom> groom, QSharedPointer<Optimize> optimize,
                                   QSharedPointer<OptimizeParameters> optimize_parameters,
                                   QSharedPointer<Session> session, double maxAngle, float decimationPercent,
                                   int numClusters)
    : type_(type),
      groom_(groom),
      optimize_(optimize),
      optimize_parameters_(optimize_parameters),
      session_(session),
      decimation_percent_(decimationPercent),
      max_angle_(maxAngle),
      num_clusters_(numClusters) {}

//---------------------------------------------------------------------------
ShapeworksWorker::~ShapeworksWorker() {}

//---------------------------------------------------------------------------
void ShapeworksWorker::process() {
  switch (this->type_) {
    case ShapeworksWorker::GroomType:
      try {
        this->groom_->run();
      } catch (itk::ExceptionObject& ex) {
        SW_LOG_ERROR(std::string("ITK Exception: ") + ex.GetDescription());
        return;
      } catch (std::runtime_error& e) {
        SW_LOG_ERROR(e.what());
        return;
      } catch (std::exception& e) {
        SW_LOG_ERROR(e.what());
        return;
      }
      if (this->groom_->get_aborted()) {
        SW_LOG_MESSAGE("Groom Aborted!");
        return;
      }
      break;
    case ShapeworksWorker::OptimizeType:
      try {
        SW_LOG_MESSAGE("Loading data...");
        this->optimize_parameters_->set_up_optimize(this->optimize_.data());
        SW_LOG_MESSAGE("Optimizing correspondence...");
        this->optimize_->Run();
      } catch (std::runtime_error e) {
        std::cerr << "Exception: " << e.what() << "\n";
        SW_LOG_ERROR(std::string("Error: ") + e.what());
        emit failure();
        emit finished();
        return;
      } catch (itk::ExceptionObject& ex) {
        std::cerr << "ITK Exception: " << ex << std::endl;
        SW_LOG_ERROR(std::string("ITK Exception: ") + ex.GetDescription());
        emit failure();
        emit finished();
        return;
      } catch (std::exception& e) {
        SW_LOG_ERROR(e.what());
        emit failure();
        emit finished();
        return;
      } catch (...) {
        SW_LOG_ERROR("Error during optimization!");
        emit failure();
        emit finished();
        return;
      }
      if (this->optimize_->GetAborted()) {
        SW_LOG_MESSAGE("Optimization Aborted!");
        emit failure();
        return;
      }

      break;
    case ShapeworksWorker::ReconstructType:
      try {
        SW_LOG_MESSAGE("Warping to mean space...");
        for (int i = 0; i < this->session_->get_domains_per_shape(); i++) {
          auto shapes = this->session_->get_shapes();
          std::vector<std::string> distance_transforms;
          std::vector<std::vector<itk::Point<double>>> local, global;
          for (auto& s : shapes) {
            distance_transforms.push_back(s->get_groomed_filename_with_path(i));
            auto particles = s->get_particles();
            local.push_back(particles.get_local_points(i));
            global.push_back(particles.get_world_points(i));
          }
          this->session_->get_mesh_manager()->get_surface_reconstructor(i)->initializeReconstruction(
              local, global, distance_transforms, this->max_angle_, this->decimation_percent_, this->num_clusters_);
        }
      } catch (std::runtime_error e) {
        if (std::string(e.what()).find_first_of("Warning") != std::string::npos) {
          SW_LOG_WARNING(e.what());
        } else {
          SW_LOG_ERROR(e.what());
          emit finished();
          return;
        }
      } catch (std::exception& e) {
        if (std::string(e.what()).find_first_of("Warning") != std::string::npos) {
          SW_LOG_WARNING(e.what());
        } else {
          SW_LOG_ERROR(e.what());
          emit finished();
          return;
        }
      } catch (...) {
        SW_LOG_ERROR("Error during optimization!");
        emit finished();
        return;
      }
      break;
  }

  emit result_ready();
  emit finished();
}

}  // namespace shapeworks
