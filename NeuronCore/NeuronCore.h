#pragma once

/* NeuronCore.h - Core neural processing layer (public API)
 * No external headers required - this is the foundation library.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the NeuronCore subsystem.
 * Returns 0 on success, non-zero on failure. */
int NeuronCore_Init(void);

/* Shut down the NeuronCore subsystem and release all resources. */
void NeuronCore_Shutdown(void);

#ifdef __cplusplus
}
#endif
