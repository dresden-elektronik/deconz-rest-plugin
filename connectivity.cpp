/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "connectivity.h"

/*! Constructor. */
Connectivity::Connectivity()
{
}

/*! Adds a rlqi value to the RLQI list.
    \param rlqi - a route lqi value
 */
void Connectivity::addToRLQIList(quint8 rlqi)
{
    rlqiList.push_back(rlqi);
}

/*! Returns the RLQI list.
 */
std::list<quint8> Connectivity::getRLQIList()
{
    return rlqiList;
}

/*! Sets the RLQI list.
    \param list - the route lqi list
 */
void Connectivity::setRLQIList(std::list<quint8> list)
{
    rlqiList = list;
}

/*! Clears the RLQI list.
 */
void Connectivity::clearRLQIList()
{
    rlqiList.clear();
}

/*! Search the global targets vector for a node with a given Address and returns the node.
    \param extAddr - the extended address of a node
    \return the searched node found in the targets vector or the coordinator if no node was found
 */
DeRestPluginPrivate::nodeVisited Connectivity::getNodeWithAddress(uint64_t extAddr)
{
    std::vector<DeRestPluginPrivate::nodeVisited>::iterator i = targets.begin();
    std::vector<DeRestPluginPrivate::nodeVisited>::iterator end = targets.end();

    for (; i != end; ++i)
    {
        if (i->node->address().ext() == extAddr)
        {
            return *i;
        }
    }
    return start;
}

/*! Search the global targets vector for a node with a given Address and returns the index.
    \param extAddr - the extended address of a node
    \return index of the searched node or -1 if node was not found
 */
int Connectivity::getIndexWithAddress(uint64_t extAddr)
{
    std::vector<DeRestPluginPrivate::nodeVisited>::iterator i = targets.begin();
    std::vector<DeRestPluginPrivate::nodeVisited>::iterator end = targets.end();

    for (int idx = 0; i != end; ++i, idx++)
    {
        if (i->node->address().ext() == extAddr)
        {
            return idx;
        }
    }
    return -1;
}

/*! The algorithm searches all routes of the graph recursively. A node structure with neighbours and a visited flag is needed.
    For each route the min LQI value is computed (the routes LQI / RLQI value) and saved in the global rlqiList.
    From this list the highest LQI value can be picked. This value describes the link quality of the best route to the gateway.

    \param path - empty vector. The found nodes that belong to a path will be saved here.
    \param current - start (and current) node
    \param target - target node
    \return void
 */
void Connectivity::searchAllPaths(std::vector<DeRestPluginPrivate::nodeVisited> &path, DeRestPluginPrivate::nodeVisited &current, DeRestPluginPrivate::nodeVisited &target)
{
    // if target node found
    if (target.node->address().ext() == current.node->address().ext())
    {
        path.push_back(target);
/*
        DBG_Printf(DBG_INFO,"found route!\n");
        for(uint i = 0; i < path.size(); ++i)
        {
             DBG_Printf(DBG_INFO,"node %u = %s !\n",(i+1),qPrintable(path[i].node->address().toStringExt()));
        }
*/
        // compute min-LQI for each route
        std::list<quint8> lqiList;
        quint8 lqi1 = 0;
        quint8 lqi2 = 0;

        for (uint k = 0; k < (path.size()-1); ++k)
        {
            DeRestPluginPrivate::nodeVisited act = path[k];
            DeRestPluginPrivate::nodeVisited next = path[k+1];

            for (uint l = 0; l < act.node->neighbors().size(); l++)
            {
                if (act.node->neighbors()[l].address().ext() == next.node->address().ext())
                {
                    //lqi value from actual node to his neighbor
                    lqi1 = act.node->neighbors()[l].lqi();
                    //DBG_Printf(DBG_INFO, "LQI1 = %u !\n", lqi1);
                    //lqi value from the opposite direction
                    DeRestPluginPrivate::nodeVisited oppositeNode = getNodeWithAddress(act.node->neighbors()[l].address().ext());

                    for(uint y = 0; y < oppositeNode.node->neighbors().size(); y++)
                    {
                        if(oppositeNode.node->neighbors()[y].address().ext() == act.node->address().ext())
                        {
                            lqi2 = oppositeNode.node->neighbors()[y].lqi();
                            //DBG_Printf(DBG_INFO, "LQI2 = %u !\n", lqi2);
                            break;
                        }
                    }

                    //push the lower lqi value to the result list (if one lqi vaule == 0: take the other)
                    /*
                    if ((lqi1 < lqi2) && (lqi1 != 0))
                    {
                        lqiList.push_back(lqi1);
                    }
                    else if (lqi2 != 0)
                    {
                        lqiList.push_back(lqi2);
                    }            
                    else if (lqi1 != 0)
                    {
                        lqiList.push_back(lqi1);
                        //alternative: compute the average
                        //lqiList.push_back(((lqi1+lqi2)/2));
                    }
                    */
                    // or the higher ? tx of usb stick very bad -> high differences of lqi values between two nodes -> difficult testing
                    if ((lqi1 > lqi2))
                    {
                        lqiList.push_back(lqi1);
                    }
                    else
                    {
                        lqiList.push_back(lqi2);
                    }
                }
            }
        }

        lqiList.sort();
        if (lqiList.front() != 0)
        {
            addToRLQIList(lqiList.front());
        }
        //DBG_Printf(DBG_INFO, "RLQI = %u !\n", lqiList.front());
    }
    else // target node not found yet
    {
        DeRestPluginPrivate::nodeVisited actNeighbor;

        path.push_back(current);
        current.visited = true;

        //its mandatory to search current in global targets vector and set it also visited!
        int currTargetIndex = getIndexWithAddress(current.node->address().ext());
        if (currTargetIndex != -1)
        {
            targets[currTargetIndex].visited = true;
        }

        for (uint j = 0; j < current.node->neighbors().size(); j++)
        {
            actNeighbor = getNodeWithAddress(current.node->neighbors()[j].address().ext());

            if (!actNeighbor.visited)
            {
                searchAllPaths(path, actNeighbor, target);
                path.pop_back();
                actNeighbor.visited = false;

                //its mandatory to search actNeighbor in targets and set it also visited false!
                int actNeighborIndex = getIndexWithAddress(actNeighbor.node->address().ext());
                if (actNeighborIndex != -1)
                {
                    targets[actNeighborIndex].visited = false;
                }
            }
        }
    }
}
