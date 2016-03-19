/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

#include <iostream>
#include <math.h>
#include "stdio.h"
#include <vector>
#include <list>
#include <deconz/dbg_trace.h>
#include <deconz/node.h>
#include "de_web_plugin_private.h"

/*! \class Connectivity

    Computes connectivity between nodes.
 */
class Connectivity
{
private:
    std::list<quint8> rlqiList;

public:
    Connectivity();

    void addToRLQIList(quint8 rlqi);
    std::list<quint8> getRLQIList();
    void setRLQIList(std::list<quint8> rlqiList);
    void clearRLQIList();
    void searchAllPaths(std::vector<DeRestPluginPrivate::nodeVisited> &path, DeRestPluginPrivate::nodeVisited &current, DeRestPluginPrivate::nodeVisited &target);
    DeRestPluginPrivate::nodeVisited getNodeWithAddress(uint64_t extAddr);
    int getIndexWithAddress(uint64_t extAddr);

    std::vector<DeRestPluginPrivate::nodeVisited> targets;
    DeRestPluginPrivate::nodeVisited start;
};


#endif // CONNECTIVITY_H
