// std
#include <iostream>

// qt
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>

// shapeworks
#include <Logging.h>
#include <Mesh/Mesh.h>
#include <Python/PythonWorker.h>

// studio
#include <Data/Session.h>
#include <Data/ShapeWorksWorker.h>
#include <Data/Worker.h>
#include <DeepSSM/DeepSSMJob.h>
#include <DeepSSM/DeepSSMParameters.h>
#include <DeepSSM/DeepSSMTool.h>
#include <Interface/ShapeWorksStudioApp.h>
#include <Interface/Style.h>
#include <Logging.h>
#include <Shape.h>

// vtk
#include <vtkPointData.h>

// ui
#include <ui_DeepSSMTool.h>

namespace shapeworks {

//---------------------------------------------------------------------------
DeepSSMTool::DeepSSMTool(Preferences& prefs) : preferences_(prefs) {
  ui_ = new Ui_DeepSSMTool;
  ui_->setupUi(this);

#ifdef Q_OS_MACOS
  ui_->tab_widget->tabBar()->setMinimumWidth(300);
  ui_->tab_widget->setStyleSheet(
      "QTabBar::tab:selected { border-radius: 4px; background-color: #3784f7; color: white; }");
  ui_->tab_widget->tabBar()->setElideMode(Qt::TextElideMode::ElideNone);
#endif

  // set to html mode
  ui_->prep_text_edit->setAcceptRichText(true);

  connect(ui_->run_button, &QPushButton::clicked, this, &DeepSSMTool::run_clicked);
  connect(ui_->restore_defaults, &QPushButton::clicked, this, &DeepSSMTool::restore_defaults);

  connect(ui_->data_open_button, &QPushButton::clicked, this, &DeepSSMTool::update_panels);
  connect(ui_->controls_open_button, &QPushButton::clicked, this, &DeepSSMTool::update_panels);
  connect(ui_->training_open_button, &QPushButton::clicked, this, &DeepSSMTool::update_panels);

  connect(ui_->generated_data_checkbox, &QCheckBox::stateChanged, this, &DeepSSMTool::show_augmentation_meshes);
  connect(ui_->original_data_checkbox, &QCheckBox::stateChanged, this, &DeepSSMTool::show_augmentation_meshes);

  connect(ui_->tab_widget, &QTabWidget::currentChanged, this, &DeepSSMTool::tab_changed);

  connect(ui_->training_fine_tuning, &QCheckBox::stateChanged, this, &DeepSSMTool::training_fine_tuning_changed);

  connect(ui_->run_all, &QCheckBox::stateChanged, this, &DeepSSMTool::update_panels);

  QIntValidator* zero_to_hundred = new QIntValidator(0, 100, this);
  ui_->validation_split->setValidator(zero_to_hundred);
  ui_->testing_split->setValidator(zero_to_hundred);

  connect(ui_->validation_split, &QLineEdit::textChanged, this, [=]() { update_split(ui_->validation_split); });
  connect(ui_->testing_split, &QLineEdit::textChanged, this, [=]() { update_split(ui_->testing_split); });

  ui_->tab_widget->setCurrentIndex(0);
  tab_changed(0);

  Style::apply_normal_button_style(ui_->restore_defaults);
  ui_->violin_plot->setText("");
  ui_->training_plot->setText("");
  update_panels();
}

//---------------------------------------------------------------------------
void DeepSSMTool::tab_changed(int tab) {
  switch (tab) {
    case 0:
      current_tool_ = DeepSSMTool::ToolMode::DeepSSM_PrepType;
      break;
    case 1:
      current_tool_ = DeepSSMTool::ToolMode::DeepSSM_AugmentationType;
      break;
    case 2:
      current_tool_ = DeepSSMTool::ToolMode::DeepSSM_TrainingType;
      break;
    case 3:
      current_tool_ = DeepSSMTool::ToolMode::DeepSSM_TestingType;
      break;
  }
  update_panels();
  update_meshes();
}

//---------------------------------------------------------------------------
DeepSSMTool::~DeepSSMTool() {}

//---------------------------------------------------------------------------
void DeepSSMTool::set_session(QSharedPointer<Session> session) {
  session_ = session;
  load_params();
  update_panels();
  update_meshes();
}

//---------------------------------------------------------------------------
void DeepSSMTool::set_app(ShapeWorksStudioApp* app) { app_ = app; }

//---------------------------------------------------------------------------
void DeepSSMTool::load_params() {
  auto params = DeepSSMParameters(session_->get_project());

  ui_->validation_split->setText(QString::number(params.get_validation_split()));
  ui_->testing_split->setText(QString::number(params.get_testing_split()));

  auto spacing = params.get_spacing();
  ui_->spacing_x->setText(QString::number(spacing[0]));
  ui_->spacing_y->setText(QString::number(spacing[1]));
  ui_->spacing_z->setText(QString::number(spacing[2]));

  ui_->num_samples->setText(QString::number(params.get_aug_num_samples()));
  ui_->percent_variability->setText(QString::number(params.get_aug_percent_variability()));
  ui_->sampler_type->setCurrentText(QString::fromStdString(params.get_aug_sampler_type()));

  ui_->training_epochs->setText(QString::number(params.get_training_epochs()));
  ui_->training_learning_rate->setText(QString::number(params.get_training_learning_rate()));
  ui_->training_decay_learning->setChecked(params.get_training_decay_learning_rate());
  ui_->training_fine_tuning->setChecked(params.get_training_fine_tuning());
  ui_->training_fine_tuning_epochs->setText(QString::number(params.get_training_fine_tuning_epochs()));
  ui_->training_batch_size->setText(QString::number(params.get_training_batch_size()));
  ui_->training_fine_tuning_learning_rate->setText(QString::number(params.get_training_fine_tuning_learning_rate()));

  ui_->prep_text_edit->setText(QString::fromStdString(params.get_prep_message()));

  update_panels();
  update_meshes();
}

//---------------------------------------------------------------------------
void DeepSSMTool::store_params() {
  auto params = DeepSSMParameters(session_->get_project());

  params.set_validation_split(ui_->validation_split->text().toDouble());
  params.set_testing_split(ui_->testing_split->text().toDouble());

  params.set_spacing(
      {ui_->spacing_x->text().toDouble(), ui_->spacing_y->text().toDouble(), ui_->spacing_z->text().toDouble()});

  params.set_aug_num_samples(ui_->num_samples->text().toInt());
  params.set_aug_percent_variability(ui_->percent_variability->text().toDouble());
  params.set_aug_sampler_type(ui_->sampler_type->currentText().toStdString());

  params.set_training_epochs(ui_->training_epochs->text().toInt());
  params.set_training_learning_rate(ui_->training_learning_rate->text().toDouble());
  params.set_training_decay_learning_rate(ui_->training_decay_learning->isChecked());
  params.set_training_fine_tuning(ui_->training_fine_tuning->isChecked());
  params.set_training_fine_tuning_epochs(ui_->training_fine_tuning_epochs->text().toInt());
  params.set_training_batch_size(ui_->training_batch_size->text().toInt());
  params.set_training_fine_tuning_learning_rate(ui_->training_fine_tuning_learning_rate->text().toDouble());
  params.save_to_project();
}

//---------------------------------------------------------------------------
void DeepSSMTool::shutdown() { app_->get_py_worker()->abort_job(); }

//---------------------------------------------------------------------------
bool DeepSSMTool::is_active() { return session_ && session_->get_tool_state() == Session::DEEPSSM_C; }

//---------------------------------------------------------------------------
void DeepSSMTool::run_clicked() {
  if (tool_is_running_) {
    ui_->run_button->setText("Aborting...");
    ui_->run_button->setEnabled(false);
    deep_ssm_->abort();
    app_->get_py_worker()->abort_job();
  } else {
    if (ui_->run_all->isChecked()) {
      run_tool(DeepSSMTool::ToolMode::DeepSSM_PrepType);
    } else {
      run_tool(current_tool_);
    }
  }
}

//---------------------------------------------------------------------------
void DeepSSMTool::handle_thread_complete() {
  try {
    if (deep_ssm_->is_aborted()) {
      ui_->prep_text_edit->setText("Aborted");
    }
    Q_EMIT progress(100);
    update_meshes();
    tool_is_running_ = false;
    update_panels();
    session_->reload_particles();

    if (!deep_ssm_->is_aborted()) {
      if (ui_->run_all->isChecked()) {
        if (current_tool_ == ToolMode::DeepSSM_PrepType) {
          run_tool(DeepSSMTool::ToolMode::DeepSSM_AugmentationType);
        } else if (current_tool_ == ToolMode::DeepSSM_AugmentationType) {
          run_tool(DeepSSMTool::ToolMode::DeepSSM_TrainingType);
        } else if (current_tool_ == ToolMode::DeepSSM_TrainingType) {
          run_tool(DeepSSMTool::ToolMode::DeepSSM_TestingType);
        }
      }
    }
  } catch (std::exception& e) {
    SW_ERROR("{}", e.what());
  }
}

//---------------------------------------------------------------------------
void DeepSSMTool::handle_progress(int val, QString message) {
  if (current_tool_ == DeepSSMTool::ToolMode::DeepSSM_PrepType) {
    ui_->prep_text_edit->setText(deep_ssm_->get_prep_message());
    ui_->prep_text_edit->setEnabled(true);
  }
  load_plots();
  update_tables();
  update_meshes();
  SW_PROGRESS(val, message.toStdString());
}

//---------------------------------------------------------------------------
void DeepSSMTool::handle_error(QString msg) {}

//---------------------------------------------------------------------------
void DeepSSMTool::update_panels() {
  ui_->data_content->setVisible(ui_->data_open_button->isChecked());
  ui_->controls_content->setVisible(ui_->controls_open_button->isChecked());
  ui_->training_content->setVisible(ui_->training_open_button->isChecked());
  ui_->tab_widget->setEnabled(!tool_is_running_);
  ui_->restore_defaults->setEnabled(!tool_is_running_);

  if (!session_ || !session_->get_project()) {
    return;
  }

  auto params = DeepSSMParameters(session_->get_project());
  QString string = "";
  ui_->data_panel->hide();
  ui_->training_panel->hide();
  bool enabled = true;
  switch (current_tool_) {
    case DeepSSMTool::ToolMode::DeepSSM_PrepType:
      string = "Groom and Optimize";
      break;
    case DeepSSMTool::ToolMode::DeepSSM_AugmentationType:
      string = "Data Augmentation";
      ui_->data_panel->show();
      enabled = params.get_prep_step_complete();
      break;
    case DeepSSMTool::ToolMode::DeepSSM_TrainingType:
      string = "Training";
      ui_->training_panel->show();
      enabled = params.get_aug_step_complete();
      break;
    case DeepSSMTool::ToolMode::DeepSSM_TestingType:
      string = "Testing";
      enabled = params.get_training_step_complete();
      break;
  }

  if (ui_->run_all->isChecked()) {
    string = "All Steps";
  }

  if (tool_is_running_) {
    Style::apply_abort_button_style(ui_->run_button);
    ui_->run_button->setText("Abort");
    ui_->run_button->setEnabled(true);
  } else {
    Style::apply_normal_button_style(ui_->run_button);
    ui_->run_button->setEnabled(enabled);
    ui_->run_button->setText("Run " + string);
  }
}

//---------------------------------------------------------------------------
void DeepSSMTool::update_split(QLineEdit* source) {}

//---------------------------------------------------------------------------
void DeepSSMTool::handle_new_mesh() {
  if (current_tool_ == DeepSSMTool::ToolMode::DeepSSM_TestingType) {
    update_testing_meshes();
  }
}

//---------------------------------------------------------------------------
void DeepSSMTool::training_fine_tuning_changed() {
  ui_->training_fine_tuning_epochs->setEnabled(ui_->training_fine_tuning->isChecked());
  ui_->training_fine_tuning_learning_rate->setEnabled(ui_->training_fine_tuning->isChecked());
}

//---------------------------------------------------------------------------
void DeepSSMTool::update_tables() {
  populate_table_from_csv(ui_->training_table, "deepssm/model/train_log.csv", true);
  populate_table_from_csv(ui_->table, "deepssm/augmentation/TotalData.csv", false);
}

//---------------------------------------------------------------------------
void DeepSSMTool::populate_table_from_csv(QTableWidget* table, QString filename, bool header) {
  table->clear();
  if (!QFile(filename).exists()) {
    return;
  }
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    SW_ERROR("Unable to open file: " + filename.toStdString());
    return;
  }

  auto data = QString(file.readAll()).trimmed();
  auto lines = data.split('\n');
  file.close();

  if (lines.empty()) {
    return;
  }
  auto headers = lines[0].split(',');
  table->setColumnCount(headers.size());
  table->horizontalHeader()->setVisible(true);
  if (header) {
    table->setHorizontalHeaderLabels(headers);
    lines.pop_front();
    table->verticalHeader()->setVisible(false);
  } else {
    table->verticalHeader()->setVisible(true);
  }
  table->setRowCount(lines.size());

  int row = 0;
  for (auto line : lines) {
    int col = 0;
    for (auto item : line.split(',')) {
      item = item.trimmed();
      if (item != "") {
        QTableWidgetItem* new_item = new QTableWidgetItem(QString(item));
        table->setItem(row, col++, new_item);
      }
    }
    row++;
  }
}

//---------------------------------------------------------------------------
QStringList DeepSSMTool::read_images_from_csv(QString filename) {
  // first column is image, no header
  QStringList list;
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    SW_ERROR("Unable to open file: " + filename.toStdString());
    return list;
  }

  auto data = QString(file.readAll()).trimmed();
  auto lines = data.split('\n');
  file.close();

  if (lines.empty()) {
    return list;
  }

  for (auto line : lines) {
    auto item = line.split(',')[0].trimmed();
    if (item != "") {
      list.push_back(item);
    }
  }

  return list;
}

//---------------------------------------------------------------------------
void DeepSSMTool::show_training_meshes() {
  shapes_.clear();

  QStringList filenames;
  filenames << "deepssm/model/examples/validation_best.particles";
  filenames << "deepssm/model/examples/validation_median.particles";
  filenames << "deepssm/model/examples/validation_worst.particles";
  filenames << "deepssm/model/examples/train_best.particles";
  filenames << "deepssm/model/examples/train_median.particles";
  filenames << "deepssm/model/examples/train_worst.particles";

  QStringList names;
  names << "best validation"
        << "median validation"
        << "worst validation";
  names << "best training"
        << "median training"
        << "worst training";

  QStringList scalar_filenames;
  scalar_filenames << "deepssm/model/examples/validation_best.scalars";
  scalar_filenames << "deepssm/model/examples/validation_median.scalars";
  scalar_filenames << "deepssm/model/examples/validation_worst.scalars";
  scalar_filenames << "deepssm/model/examples/train_best.scalars";
  scalar_filenames << "deepssm/model/examples/train_median.scalars";
  scalar_filenames << "deepssm/model/examples/train_worst.scalars";

  QStringList index_filenames;
  index_filenames << "deepssm/model/examples/validation_best.index";
  index_filenames << "deepssm/model/examples/validation_median.index";
  index_filenames << "deepssm/model/examples/validation_worst.index";
  index_filenames << "deepssm/model/examples/train_best.index";
  index_filenames << "deepssm/model/examples/train_median.index";
  index_filenames << "deepssm/model/examples/train_worst.index";

  std::vector<bool> validation = {true, true, true, false, false, false};

  auto all_shapes = session_->get_shapes();
  auto all_subjects = session_->get_project()->get_subjects();

  std::string feature_name = session_->get_image_name();

  for (int i = 0; i < names.size(); i++) {
    if (QFileInfo(filenames[i]).exists()) {
      ShapeHandle shape = ShapeHandle(new Shape());
      auto subject = std::make_shared<Subject>();
      shape->set_subject(subject);
      shape->set_mesh_manager(session_->get_mesh_manager());
      shape->import_local_point_files({filenames[i].toStdString()});
      shape->import_global_point_files({filenames[i].toStdString()});
      shape->load_feature_from_scalar_file(scalar_filenames[i].toStdString(), "deepssm_error");
      shape->get_reconstructed_meshes();

      // read index from file
      std::ifstream index_file(index_filenames[i].toStdString());
      if (index_file.is_open()) {
        std::string index_string;
        std::getline(index_file, index_string);
        index_file.close();
        std::string image_filename = index_string;

        if (validation[i]) {
          image_filename = "deepssm/val_and_test_images/" + image_filename + ".nrrd";
        } else {
          if (image_filename.find("Generated") != std::string::npos) {
            image_filename = "deepssm/augmentation/Generated-Images/" + image_filename + ".nrrd";
          } else {
            image_filename = "deepssm/train_images/" + image_filename + ".nrrd";
          }
        }

        // if the filename doesn't exist, print a warning
        if (!QFileInfo(QString::fromStdString(image_filename)).exists()) {
          SW_WARN("File doesn't exist: {}", image_filename);
        } else {
          project::types::StringMap map;
          map[feature_name] = image_filename;
          subject->set_feature_filenames(map);
        }
      }

      std::vector<std::string> list;
      list.push_back(names[i].toStdString());
      list.push_back("");
      list.push_back("");
      list.push_back("");
      shape->set_annotations(list);

      shapes_.push_back(shape);
    }
  }
  Q_EMIT update_view();
}

//---------------------------------------------------------------------------
void DeepSSMTool::show_testing_meshes() {
  shapes_.clear();
  deep_ssm_ = QSharedPointer<DeepSSMJob>::create(session_, DeepSSMTool::ToolMode::DeepSSM_TestingType);
  auto id_list = get_split(session_->get_project(), SplitType::TEST);

  auto subjects = session_->get_project()->get_subjects();
  auto shapes = session_->get_shapes();
  std::string feature_name = session_->get_image_name();

  for (auto& id : id_list) {
    QString filename =
        QString("deepssm/model/test_predictions/FT_Predictions/predicted_ft_") + QString::number(id) + ".particles";

    if (QFileInfo(filename).exists()) {
      ShapeHandle shape = ShapeHandle(new Shape());
      auto subject = std::make_shared<Subject>();
      subject->set_display_name(shapes[id]->get_display_name());
      shape->set_subject(subject);
      shape->set_mesh_manager(session_->get_mesh_manager());
      shape->import_local_point_files({filename.toStdString()});
      shape->import_global_point_files({filename.toStdString()});

      auto image_filename = "deepssm/val_and_test_images/" + std::to_string(id) + ".nrrd";
      project::types::StringMap map;
      map[feature_name] = image_filename;
      subject->set_feature_filenames(map);

      /// subject->set_feature_filenames(subjects[id]->get_feature_filenames());
      shape->get_reconstructed_meshes();
      std::vector<std::string> list;
      list.push_back(shapes[id]->get_annotations()[0]);
      list.push_back("");
      list.push_back("");
      list.push_back("");
      shape->set_annotations(list);

      shapes_.push_back(shape);
    }
  }

  update_testing_meshes();

  Q_EMIT update_view();
}

//---------------------------------------------------------------------------
void DeepSSMTool::update_testing_meshes() {
  try {
    deep_ssm_ = QSharedPointer<DeepSSMJob>::create(session_, DeepSSMTool::ToolMode::DeepSSM_TestingType);
    auto id_list = get_split(session_->get_project(), SplitType::TEST);

    auto subjects = session_->get_project()->get_subjects();
    auto shapes = session_->get_shapes();

    QTableWidget* table = ui_->testing_table;

    table->clear();
    QStringList headers;
    headers << "name"
            << "average distance";
    table->setColumnCount(headers.size());
    table->horizontalHeader()->setVisible(true);
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->setRowCount(id_list.size());

    int idx = -1;
    for (auto& id : id_list) {
      idx++;

      auto name = QString::fromStdString(subjects[id]->get_display_name());

      QTableWidgetItem* new_item = new QTableWidgetItem(QString(name));

      table->setItem(idx, 0, new_item);

      auto mesh_group = shapes[id]->get_original_meshes(true);
      if (!mesh_group.valid()) {
        // SW_WARN("Warning: Couldn't load groomed mesh for " + name.toStdString());
        QTableWidgetItem* new_item = new QTableWidgetItem("inference");
        table->setItem(idx, 1, new_item);
        continue;
      }
      Mesh base(mesh_group.meshes()[0]->get_poly_data());

      // transform base by registration transforms
      auto extra_values = subjects[id]->get_extra_values();
      if (extra_values.count("registration_transform")) {
        std::string transform = extra_values["registration_transform"];
        // convert to vtkTransform
        auto transform_matrix = ProjectUtils::convert_transform(transform);
        base.applyTransform(transform_matrix);
      }

      std::string filename =
          "deepssm/model/test_predictions/FT_Predictions/predicted_ft_" + std::to_string(id) + ".particles";
      if (QFileInfo(QString::fromStdString(filename)).exists()) {
        if (idx < shapes_.size()) {  // test shapes
          auto shape = shapes_[idx];
          MeshGroup group = shape->get_reconstructed_meshes();
          if (group.valid()) {
            Mesh m(group.meshes()[0]->get_poly_data());
            auto field = m.distance(base)[0];
            field->SetName("deepssm_error");

            double average_distance = mean(field);

            QTableWidgetItem* new_item = new QTableWidgetItem(QString::number(average_distance));
            table->setItem(idx, 1, new_item);

            group.meshes()[0]->get_poly_data()->GetPointData()->AddArray(field);
          } else {
            QTableWidgetItem* new_item = new QTableWidgetItem("computing...");
            table->setItem(idx, 1, new_item);
          }
        }
      }
    }

  } catch (std::exception& e) {
    SW_ERROR("{}", e.what());
  }

  Q_EMIT update_view();
}

//---------------------------------------------------------------------------
void DeepSSMTool::update_meshes() {
  if (!is_active()) {
    return;
  }
  switch (current_tool_) {
    case DeepSSMTool::ToolMode::DeepSSM_PrepType:
      shapes_.clear();
      Q_EMIT update_view();
      break;
    case DeepSSMTool::ToolMode::DeepSSM_AugmentationType:
      show_augmentation_meshes();
      break;
    case DeepSSMTool::ToolMode::DeepSSM_TrainingType:
      show_training_meshes();
      break;
    case DeepSSMTool::ToolMode::DeepSSM_TestingType:
      show_testing_meshes();
      break;
  }
}

//---------------------------------------------------------------------------
void DeepSSMTool::show_augmentation_meshes() {
  update_tables();
  shapes_.clear();

  QString filename = "deepssm/augmentation/TotalData.csv";
  if (QFile(filename).exists()) {
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly)) {
      SW_ERROR("Unable to open file: " + filename.toStdString());
      return;
    }

    int row = 0;
    while (!file.atEnd()) {
      QByteArray line = file.readLine();
      bool show_generated = ui_->generated_data_checkbox->isChecked();
      bool show_original = ui_->original_data_checkbox->isChecked();
      // this needs to be replaced with it's own column (in all the python code as well)
      bool is_generated = line.contains("Generated");
      if ((is_generated && show_generated) || (!is_generated && show_original)) {
        auto image_file = line.split(',')[0].toStdString();
        std::string particle_file = (line.split(',')[1]).toStdString();

        auto subject = std::make_shared<Subject>();
        ShapeHandle shape = ShapeHandle(new Shape());
        shape->set_subject(subject);
        shape->set_mesh_manager(session_->get_mesh_manager());
        shape->import_local_point_files({particle_file});
        shape->import_global_point_files({particle_file});

        shape->get_reconstructed_meshes();

        std::vector<std::string> list;
        list.push_back((QFileInfo(QString::fromStdString(particle_file)).baseName()).toStdString());
        list.push_back("");
        list.push_back("");
        list.push_back("");
        shape->set_annotations(list);

        shapes_.push_back(shape);
        row++;
      }
    }
  }
  load_plots();
  Q_EMIT update_view();
}

//---------------------------------------------------------------------------
ShapeList DeepSSMTool::get_shapes() { return shapes_; }

//---------------------------------------------------------------------------
void DeepSSMTool::load_plots() {
  violin_plot_ = load_plot("deepssm/augmentation/violin.png");
  training_plot_ = load_plot("deepssm/model/training_plot.png");
  resize_plots();
}

//---------------------------------------------------------------------------
QPixmap DeepSSMTool::load_plot(QString filename) {
  if (QFile(filename).exists()) {
    return QPixmap(filename);
  }
  return QPixmap();
}

//---------------------------------------------------------------------------
void DeepSSMTool::resize_plots() {
  set_plot(ui_->violin_plot, violin_plot_);
  set_plot(ui_->training_plot, training_plot_);
}

//---------------------------------------------------------------------------
void DeepSSMTool::set_plot(QLabel* qlabel, QPixmap pixmap) {
  if (!pixmap.isNull()) {
    QPixmap resized = pixmap.scaledToWidth(width() * 0.95, Qt::SmoothTransformation);
    qlabel->setPixmap(resized);
  } else {
    qlabel->setPixmap(QPixmap{});
  }
}

//---------------------------------------------------------------------------
void DeepSSMTool::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  resize_plots();
}

//---------------------------------------------------------------------------
std::string DeepSSMTool::get_display_feature() {
  if (current_tool_ == DeepSSMTool::ToolMode::DeepSSM_TrainingType ||
      current_tool_ == DeepSSMTool::ToolMode::DeepSSM_TestingType) {
    return "deepssm_error";
  }
  return "";
}

//---------------------------------------------------------------------------
std::vector<int> DeepSSMTool::get_split(ProjectHandle project, SplitType split_type) {
  auto subjects = project->get_subjects();

  std::vector<int> list;

  for (int id = 0; id < subjects.size(); id++) {
    auto extra_values = subjects[id]->get_extra_values();

    std::string split = extra_values["split"];

    if (split_type == SplitType::TRAIN) {
      if (split != "train") {
        continue;
      }
    } else if (split_type == SplitType::VAL) {
      if (split != "val") {
        continue;
      }
    } else if (split_type == SplitType::TEST) {
      if (split != "test") {
        continue;
      }
    }

    list.push_back(id);
  }
  return list;
}

//---------------------------------------------------------------------------
void DeepSSMTool::restore_defaults() {
  // need to save values from the other pages
  store_params();

  auto params = DeepSSMParameters(session_->get_project());

  switch (current_tool_) {
    case DeepSSMTool::ToolMode::DeepSSM_PrepType:
      params.restore_split_defaults();
      break;
    case DeepSSMTool::ToolMode::DeepSSM_AugmentationType:
      params.restore_augmentation_defaults();
      break;
    case DeepSSMTool::ToolMode::DeepSSM_TrainingType:
      params.restore_training_defaults();
      break;
    case DeepSSMTool::ToolMode::DeepSSM_TestingType:
      // params.restore_inference_defaults();
      break;
  }

  session_->get_project()->clear_parameters(Parameters::DEEPSSM_PARAMS);
  params.save_to_project();
  load_params();
}

//---------------------------------------------------------------------------
void DeepSSMTool::run_tool(DeepSSMTool::ToolMode type) {
  current_tool_ = type;
  Q_EMIT progress(-1);

  if (type == DeepSSMTool::ToolMode::DeepSSM_AugmentationType) {
    ui_->tab_widget->setCurrentIndex(1);

    SW_LOG("Please Wait: Running Data Augmentation...");
    // clean
    QFile("deepssm/augmentation/TotalData.csv").remove();
  } else if (type == DeepSSMTool::ToolMode::DeepSSM_TrainingType) {
    ui_->tab_widget->setCurrentIndex(2);
    SW_LOG("Please Wait: Running Training...");

    // clean
    QDir dir("deepssm/model");
    dir.removeRecursively();

    show_training_meshes();
  } else if (type == DeepSSMTool::ToolMode::DeepSSM_TestingType) {
    ui_->tab_widget->setCurrentIndex(3);

    SW_LOG("Please Wait: Running Testing...");
  } else if (type == DeepSSMTool::ToolMode::DeepSSM_PrepType) {
    ui_->tab_widget->setCurrentIndex(0);

    SW_LOG("Please Wait: Running Groom/Optimize...");
  } else {
    SW_ERROR("Unknown tool mode");
    Q_EMIT progress(100);
    return;
  }

  load_plots();
  update_tables();

  timer_.start();

  tool_is_running_ = true;
  update_panels();

  store_params();

  deep_ssm_ = QSharedPointer<DeepSSMJob>::create(session_, type);
  connect(deep_ssm_.data(), &DeepSSMJob::progress, this, &DeepSSMTool::handle_progress);
  connect(deep_ssm_.data(), &DeepSSMJob::finished, this, &DeepSSMTool::handle_thread_complete);

  app_->get_py_worker()->run_job(deep_ssm_);

  // ensure someone doesn't accidentally abort right after clicking RUN
  ui_->run_button->setEnabled(false);
  QTimer::singleShot(1000, this, [=]() { ui_->run_button->setEnabled(true); });
}

//---------------------------------------------------------------------------
}  // namespace shapeworks
