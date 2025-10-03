#ifndef USERDEVICE_H
#define USERDEVICE_H

#include "nodeprop.h"
#include "MessageHelper.h"

int  get_maxdatasize();
void open_device(NodeProp& nodeprop);
void close_device(NodeProp& nodeprop);
void init_device(NodeProp& nodeprop);
int  wait_device(NodeProp& nodeprop);
int  read_device(NodeProp& nodeprop, unsigned int *data, int& len);
void finalize_device(NodeProp& nodeprop);

#endif
