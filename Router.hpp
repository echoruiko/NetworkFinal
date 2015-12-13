//
//  Router.hpp
//  Network
//
//  Created by RuijiaSun on 11/5/15.
//  Copyright © 2015 RuijiaSun. All rights reserved.
//

#ifndef Router_h
#define Router_h

#include <stdio.h>
#include <iostream>
#include <utility>
#include <vector>
#include <queue>
#include <math.h>
#include "Link.hpp"
using namespace std;

extern const int DATA_SIZE;
extern const int ACK_SIZE;

class Routing {
public:
    float cost;
    int nextHop;
    int portNo;
    
    Routing(float costArg, int nextHopArg, int portNoArg): cost(costArg), nextHop(nextHopArg), portNo(portNoArg) {}
};

class Port {
public:
    Node n;
    Link *l;
    int me;
    
    Port() {}
    Port(Node nArg, Link *lArg, int meArg): n(nArg), l(lArg), me(meArg) {}
    Port(const Port &p) {
        n = p.n;
        l = p.l;
        me = p.me;
    }
};

class Router {
public:
    int routerID;
    vector<Routing> routingTable;
    vector<float> costs;
    vector<float> tempCosts;
    vector<Port> ports;
    vector<int> neighbors;
    vector<float> nbCosts;
    int version;
    
    bool initial;
    
    // constructor
    Router(int ID, vector<Port> ports);
    /* send ACK or CostInfo
     * return the pointer of that pkt
     */
    Packet * send(pktType type, float time, int portNo = 0, CostPkt * cp = NULL);
    int forward(Packet * p, float t);
    void computeCost();
    bool updateRT(CostPkt * cp);
    bool isNeighbor(int i);
    int getNbIndex(int no);
    int getPortNo(Node &i);
    void print();
};

#endif /* Router_h */
