#pragma once

#include <ATen/Config.h>
#include <cstddef>

// Some Fortran ABIs pass CHARACTER lengths after the explicit arguments.
#if AT_BLAS_FORTRAN_STRLEN_END()
#define AT_FORTRAN_CHARLEN(...) , __VA_ARGS__
#else
#define AT_FORTRAN_CHARLEN(...)
#endif
