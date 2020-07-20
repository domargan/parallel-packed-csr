//
// Created by menges on 7/20/20.
//

#include <vector>

using namespace std;

#ifndef PARALLEL_PACKED_CSR_BFS_H
#define PARALLEL_PACKED_CSR_BFS_H

template<typename T>
vector<uint32_t> bfs(T &graph, uint32_t start_node) {
    uint64_t n = graph.get_n();
    vector<uint32_t> out(n, UINT32_MAX);
    queue<uint32_t> next;
    next.push(start_node);
    out[start_node] = 0;

    while (!next.empty()) {
        uint32_t active = next.front();
        next.pop();

        uint32_t start = graph.nodes[active].beginning;
        uint32_t end = graph.nodes[active].end;

        // get neighbors
        // start at +1 for the sentinel
        for (int j = start + 1; j < end; j++) {
            if (!(graph.edges.items[j].value == 0) && out[graph.edges.items[j].dest] == UINT32_MAX) {
                next.push(graph.edges.items[j].dest);
                out[graph.edges.items[j].dest] = out[active] + 1;
            }
        }
    }
    return out;
}

#endif //PARALLEL_PACKED_CSR_BFS_H
