"""Read L1S2 authored navigation from the live arrival toward the next exit.

The graph belongs to NPC navigation; its links are leads for ordinary player
replay, not proof that a player can traverse a given edge.
"""

import heapq
import math
import struct

from inspect_navigation_records import inspect, sections


level, payload = next((level, data) for level, data in sections()
                      if level['file'].lower() == 'l1s2.rfl')
nodes = inspect(payload, level['version'])
positions = [struct.unpack('<3f', node['position']) for node in nodes]
edges = [set(node['neighbors']) for node in nodes]
for index, neighbors in enumerate(edges):
    for neighbor in tuple(neighbors):
        edges[neighbor].add(index)


def nearest(point, count=8):
    return sorted(range(len(positions)), key=lambda i: math.dist(point, positions[i]))[:count]


def path_between(start, goal):
    ends = set(nearest(goal, 3))
    queue = []
    distance = {}
    previous = {}
    for index in nearest(start, 3):
        distance[index] = math.dist(start, positions[index])
        heapq.heappush(queue, (distance[index], index))
    best = None
    while queue:
        cost, index = heapq.heappop(queue)
        if cost != distance[index]:
            continue
        if index in ends:
            candidate = (cost + math.dist(positions[index], goal), index)
            if best is None or candidate < best:
                best = candidate
        if best and cost > best[0]:
            break
        for next_index in edges[index]:
            next_cost = cost + math.dist(positions[index], positions[next_index])
            if next_cost < distance.get(next_index, math.inf):
                distance[next_index] = next_cost
                previous[next_index] = index
                heapq.heappush(queue, (next_cost, next_index))
    if best is None:
        return None
    chain = [best[1]]
    while chain[-1] in previous:
        chain.append(previous[chain[-1]])
    chain.reverse()
    return best[0], chain


points = [
    ('arrival', (-29.180120, -13.120156, -95.951271)),
    ('far_shore', (12.448607, -10.118479, -85.222588)),
    ('driller', (109.0512, -3.57238, -12.2843)),
    ('exit', (58.7138329, -2.08664155, 95.1859741)),
]
for (_, start), (label, goal) in zip(points, points[1:]):
    found = path_between(start, goal)
    print(label, 'start-nearest', [(i, nodes[i]['uid'], tuple(round(v, 2) for v in positions[i]))
                                  for i in nearest(start, 3)])
    if found is None:
        print(label, 'no graph route')
        continue
    cost, chain = found
    print(label, 'distance', round(cost, 2), 'nodes', len(chain))
    for index in chain:
        print(index, nodes[index]['uid'], tuple(round(v, 2) for v in positions[index]))

membership = [-1] * len(nodes)
components = []
for source in range(len(nodes)):
    if membership[source] >= 0:
        continue
    group = len(components)
    pending = [source]
    membership[source] = group
    component = []
    while pending:
        index = pending.pop()
        component.append(index)
        for neighbor in edges[index]:
            if membership[neighbor] < 0:
                membership[neighbor] = group
                pending.append(neighbor)
    components.append(component)

for label, point in points:
    index = nearest(point, 1)[0]
    group = membership[index]
    print(label, 'component', group, 'size', len(components[group]),
          'nearest', index, nodes[index]['uid'])
arrival_group = membership[nearest(points[0][1], 1)[0]]
driller_group = membership[nearest(points[2][1], 1)[0]]
if arrival_group != driller_group:
    gaps = sorted((math.dist(positions[a], positions[b]), a, b)
                  for a in components[arrival_group]
                  for b in components[driller_group])[:8]
    for distance, a, b in gaps:
        print('component-gap', round(distance, 2),
              (a, nodes[a]['uid'], tuple(round(v, 2) for v in positions[a])),
              (b, nodes[b]['uid'], tuple(round(v, 2) for v in positions[b])))
