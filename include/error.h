#pragma once

#define SCAPE_ELAST (1) /* Used by the thread safe persistent queue to signal the last time an element is returned. */
#define SCAPE_EOK (0) /* OK */
#define SCAPE_EINVAL (-1) /* Invalid argument */
#define SCAPE_EAGAIN (-2) /* Resource temporarily unavailable */
#define SCAPE_EOVERFLOW (-3) /* Overflow */
#define SCAPE_ENOTREADY (-4) /* Resource not ready */
#define SCAPE_EINTERRUPTED (-5) /* Operation interrupted */
#define SCAPE_EIO (-6) /* I/O error */
#define SCAPE_EEXTERNAL (-7) /* External errors, e.g. from OS-level operations */
#define SCAPE_EUNSUPPORTED (-8) /* Operation not supported by the device. */
#define SCAPE_EINTERNAL (-9) /* Internal errors, cannot be addressed by the user. */
