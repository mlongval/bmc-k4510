/* K4510 shim: VICE asks its resource system for two settings.  Ours answers
 * from the machine's own sound settings; see fastsid_k4510.c. */
#ifndef K4510_VICE_RESOURCES_H
#define K4510_VICE_RESOURCES_H
int resources_get_int(const char *name, int *value);
#endif
