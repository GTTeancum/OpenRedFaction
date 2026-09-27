"""Inspect authored L1S1 navigation links from the first handoff to exit9019.

This is an offline asset audit, not a player pathfinder or gameplay proof.
"""

import heapq
import math
import struct

from inspect_navigation_records import inspect, sections


level, payload = next((level, data) for level, data in sections()
                      if level['file'].lower() == 'l1s1.rfl')
nodes = inspect(payload, level['version'])
positions = [struct.unpack('<3f', node['position']) for node in nodes]
start = (-75.388870, -8.095128, 27.786995)  # real-spawn replay frame2380
goal = (112.159729, 18.864132, -48.439453)  # exit9019 walking fixture


def nearest(point, count=8):
    return [index for _, index in sorted((math.dist(point, position), i)
                                         for i, position in enumerate(positions))[:count]]


def route(source, target, adjacency):
    queue = [(0.0, source)]
    distance = {source: 0.0}
    previous = {}
    while queue:
        cost, index = heapq.heappop(queue)
        if cost != distance[index]:
            continue
        if index == target:
            path = [target]
            while path[-1] != source:
                path.append(previous[path[-1]])
            return cost, list(reversed(path))
        for neighbor in adjacency[index]:
            next_cost = cost + math.dist(positions[index], positions[neighbor])
            if next_cost < distance.get(neighbor, math.inf):
                distance[neighbor] = next_cost
                previous[neighbor] = index
                heapq.heappush(queue, (next_cost, neighbor))
    return math.inf, []


directed = [node['neighbors'] for node in nodes]
undirected = [set(neighbors) for neighbors in directed]
for index, neighbors in enumerate(directed):
    for neighbor in neighbors:
        undirected[neighbor].add(index)
for label, adjacency in [('directed', directed), ('undirected', undirected)]:
    candidates = [(cost, source, target, path)
                  for source in nearest(start)
                  for target in nearest(goal)
                  for cost, path in [route(source, target, adjacency)] if path]
    if not candidates:
        print(label, 'no authored navigation route joins the two neighborhoods')
        continue
    cost, source, target, path = min(candidates)
    print(label, 'start', start, 'nearest', source, positions[source])
    print(label, 'goal', goal, 'nearest', target, positions[target])
    print(label, 'authored_graph_distance', round(cost, 3), 'nodes', len(path))
    for index in path:
        print(index, nodes[index]['uid'], tuple(round(v, 3) for v in positions[index]))

components = []
ownership = [-1] * len(nodes)
for first in range(len(nodes)):
    if ownership[first] >= 0:
        continue
    component = len(components)
    pending = [first]
    ownership[first] = component
    members = []
    while pending:
        index = pending.pop()
        members.append(index)
        for neighbor in undirected[index]:
            if ownership[neighbor] < 0:
                ownership[neighbor] = component
                pending.append(neighbor)
    components.append(members)

source_component = ownership[nearest(start, 1)[0]]
target_component = ownership[nearest(goal, 1)[0]]
print('components', len(components), 'start', source_component,
      len(components[source_component]), 'goal', target_component,
      len(components[target_component]))
if source_component != target_component:
    distance, source, target = min((math.dist(positions[a], positions[b]), a, b)
                                   for a in components[source_component]
                                   for b in components[target_component])
    print('closest_component_gap', round(distance, 3), source,
          positions[source], target, positions[target])
    for label, first, last in [('handoff_to_gap', nearest(start, 1)[0], source),
                               ('gap_to_exit', target, nearest(goal, 1)[0])]:
        cost, path = route(first, last, undirected)
        print(label, round(cost, 3), [(i, tuple(round(v, 2) for v in positions[i]))
                                       for i in path])
