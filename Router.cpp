//
//  Router.cpp
//  Network
//
//  Created by RuijiaSun on 11/24/15.
//  Copyright © 2015 RuijiaSun. All rights reserved.
//

#include "Router.hpp"
extern vector<FILE *> PacketLossFiles;
extern vector<string> PacketLossNames;

Router::Router(int ID, vector<Port> portsArg): routerID(ID), ports(portsArg) {
    initial = true;
    version = 1;
    for (vector<Port>::iterator ip = ports.begin(); ip != ports.end(); ++ip) {
        if (ip->n.type == ROUTER) {
            neighbors.push_back(ip->n.no);
            nbCosts.push_back(ip->l->delay * ip->l->rate);
        }
    }
}
bool Router::isNeighbor(int i) {
    for (int j = 0; j < neighbors.size(); ++j) {
        if (i == neighbors[j]) return true;
    }
    return false;
}
int Router::getNbIndex(int no) {
    int res = -1;
    for (int j = 0; j < neighbors.size(); ++j) {
        if (no == neighbors[j]) return j;
    }
    return res;
}
int Router::getPortNo(Node &i) {
    for (int j = 0; j < ports.size(); ++j) {
        if (ports[j].n.no == i.no && ports[j].n.type == i.type)
            return j;
    }
    return -1;
}
Packet * Router::send(pktType type, float time,  int portNo, CostPkt * cp) {
    Packet *p = nullptr;
    Port pt;
    int size = DATA_SIZE;
    
    // if costpkt is not NULL
    if (cp != NULL) {
        int num = (type == ACK) ? getPortNo(cp->srcNode) : getPortNo(cp->destNode);
        if (num == -1) return NULL;
        pt = ports[num];
    }
    else {
        pt = ports[portNo];
    }
    if (type == ACK) {
        p = new ACKPkt(cp->ID, time, time, pt.me, NULL, cp);
        size = ACK_SIZE;
    }
    else if (type == CostInfo) {
        int id = (cp == NULL) ? 1 : cp->ID+1;
        for (int i = 0; i < routingTable.size(); ++i) {
            if (routingTable[i].nextHop == pt.n.no && i != pt.n.no)
                tempCosts[i] = INFINITY;
            else
                tempCosts[i] = costs[i];
        }
        p = new CostPkt(id, time, time, pt.l->linkID, pt.me, routerID, pt.n.no, version, tempCosts);
        size = DATA_SIZE;
    }
    if (pt.l->buffer_caps[pt.me] + size <= pt.l->buffer_size) {
        if (pt.me == 0) pt.l->buffer1.push(p);
        else if (pt.me == 1) pt.l->buffer2.push(p);
        pt.l->buffer_caps[pt.me] += size;
    }
    else {
        cout<<"router loss "<< p->type<<" p from "<<p->srcNode.no <<" to "<<p->destNode.no<<" is lost\n";
    }
    return p;
}

//
void Router::computeCost() {
    for (int i = 0; i < ports.size(); ++i) {
        if (ports[i].n.type == ROUTER) {
            
            // update neighbors first A is the neighbor, no = A
            // we first update B->A->...->A
            
            int no = ports[i].n.no;
            float newCost = ports[i].l->cost + ports[i].l->delay * ports[i].l->rate;
            float deltCost = newCost - nbCosts[getNbIndex(no)];
            nbCosts[getNbIndex(no)] = newCost;
            for (int j = 0; j < routingTable.size(); ++j) {
                if (routingTable[j].nextHop == no) routingTable[j].cost += deltCost;
                
            }
        }
    }
    // A->B->C may change into A->C, we are comparing.
    for (int i = 0; i < routingTable.size(); ++i) {
        if (isNeighbor(i) && routingTable[i].nextHop != i) {
            if (routingTable[i].cost > nbCosts[getNbIndex(i)]) {
                routingTable[i].nextHop = i;
                routingTable[i].cost = nbCosts[getNbIndex(i)];
                Node n(i, ROUTER);
                routingTable[i].portNo = getPortNo(n);
            }
        }
    }
    for (int i = 0; i < costs.size(); ++i) {
        costs[i] = routingTable[i].cost;
    }
}

int Router::forward(Packet * p, float t) {
    int portNo = 0;
    if (p->destRouter == routerID) {
        portNo = getPortNo(p->destNode);
    }
    else {
        int no = p->destRouter;
        portNo = routingTable[no].portNo;
    }
    int me = ports[portNo].me;
    Link * lk = ports[portNo].l;
    p->me = me;
    int size = (p->type == Data) ? DATA_SIZE : ACK_SIZE;
    if (lk->buffer_caps[me] + size <= lk->buffer_size) {
        if (me == 0) lk->buffer1.push(p);
        else if (me == 1) lk->buffer2.push(p);
        lk->buffer_caps[me] += size;
    }
    else {
        cout<<"router "<< routerID <<" lose "<< p->ID<<" p from "<<p->srcNode.no<<" at time"<< t<<endl;
        PacketLossFiles[lk->linkID] = fopen (PacketLossNames[lk->linkID].c_str(), "a");
        fprintf(PacketLossFiles[lk->linkID] , "%-10.2f %-10d\n", t, 1);
        fclose(PacketLossFiles[lk->linkID] );
    }
    return lk->linkID;
}

/* me = A, you = B, B must be the neightbor of A, B must be in the nextHop
 * first find all entries whoes nextHop is B, update
 * if A->B->...->C 's cost changes, then find all entries whose nextHop is C, enqueue.
 * first change the cost of A->C->...->other, then compare with A->B->...->other
 * until the queue is empty
 */
bool Router::updateRT(CostPkt * cp) {
    bool changed = false;
    // routerNo sends me this cost Info
    int you = cp->srcRouter;
    // cost between me and the router who sent this pkt.
    float cost_me_you = cp->costInfo[routerID];
    Node n(you, ROUTER);
    int pt = getPortNo(n);
    if (pt == -1) return false;
    // update
    queue<pair<int, float>> q;
    // find all whose nextHop is B, udpate
    for (int i = 0; i < routingTable.size(); ++i) {
        float deltCost = 0;
        if (routingTable[i].nextHop == you) {
            float newCost = cost_me_you + cp->costInfo[i];
            deltCost = newCost - routingTable[i].cost;
            if (fabsf(deltCost - 0) < 1e-7) continue;
            routingTable[i].cost = newCost;
            changed = true;
        }
    }
    // for nextHop is not B
    for (int i = 0; i < routingTable.size(); ++i) {
        if (routingTable[i].nextHop != you) {
            float cost = cost_me_you + cp->costInfo[i];
            if (routingTable[i].cost > cost) {
                routingTable[i].portNo = pt;
                routingTable[i].cost = cost;
                routingTable[i].nextHop = you;
                changed = true;
            }
        }
    }
    // change nbCosts
    if (changed) nbCosts[getNbIndex(you)] = cost_me_you;
    // update costs;
    for (int i = 0; i < routingTable.size(); ++i) {
        costs[i] = routingTable[i].cost;
    }
    return changed;
}

void Router::print() {
    cout<<routerID<<"\'s routing table\n";
    cout<<"dest    "<<"cost    "<<"nextHop    "<<endl;
    for (vector<Routing>::iterator ir = routingTable.begin(); ir != routingTable.end(); ++ir) {
        cout<<ir-routingTable.begin()<<"    "<<ir->cost<<"     "<<ir->nextHop<<endl;
    }
}
