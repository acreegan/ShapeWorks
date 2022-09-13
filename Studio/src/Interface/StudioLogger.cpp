#include "StudioLogger.h"

#include "Logging.h"

using namespace shapeworks;

//---------------------------------------------------------------------------
void StudioLogger::register_callbacks() {
  auto error_callback = std::bind(&StudioLogger::handle_error, this, std::placeholders::_1);
  Logging::Instance().set_error_callback(error_callback);
  auto message_callback = std::bind(&StudioLogger::handle_message, this, std::placeholders::_1);
  Logging::Instance().set_message_callback(message_callback);
  auto warning_callback = std::bind(&StudioLogger::handle_warning, this, std::placeholders::_1);
  Logging::Instance().set_warning_callback(warning_callback);
  auto debug_callback = std::bind(&StudioLogger::handle_debug, this, std::placeholders::_1);
  Logging::Instance().set_debug_callback(debug_callback);
}

//---------------------------------------------------------------------------
void StudioLogger::handle_message(std::string str) { Q_EMIT message(str); }

//---------------------------------------------------------------------------
void StudioLogger::handle_error(std::string str) { Q_EMIT error(str); }

//---------------------------------------------------------------------------
void StudioLogger::handle_warning(std::string str) { Q_EMIT warning(str); }

//---------------------------------------------------------------------------
void StudioLogger::handle_debug(std::string str) { Q_EMIT debug(str); }
