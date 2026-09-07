#pragma once

namespace sbc {

enum ExitCode : int {
  Success = 0,
  GeneralError = 1,
  InvalidArguments = 2,
  InputNotFound = 3,
  UnsupportedFileType = 4,
  OutputError = 5,
  BuildError = 6,
  RuntimeError = 7
};

} // namespace sbc
