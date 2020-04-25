#ifndef INCLUDED_TEGRABL_TOS_H
#define INCLUDED_TEGRABL_TOS_H

#include <tegrabl_error.h>

/**
 * @brief Set tos param in kernel command line
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_send_tos_param(void);

#endif /* INCLUDED_TEGRABL_TOS_H */
