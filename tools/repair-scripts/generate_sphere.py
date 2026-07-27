import math

def generate_icosphere(radius, subdivisions=2):
    t = (1.0 + math.sqrt(5.0)) / 2.0
    vertices = [
        (-1,  t,  0), ( 1,  t,  0), (-1, -t,  0), ( 1, -t,  0),
        ( 0, -1,  t), ( 0,  1,  t), ( 0, -1, -t), ( 0,  1, -t),
        ( t,  0, -1), ( t,  0,  1), (-t,  0, -1), (-t,  0,  1)
    ]
    
    # Normalize and scale
    vertices = [(x/math.sqrt(1+t*t)*radius, y/math.sqrt(1+t*t)*radius, z/math.sqrt(1+t*t)*radius) for x,y,z in vertices]

    faces = [
        (0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
        (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
        (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
        (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)
    ]
    
    # Subdivide
    for _ in range(subdivisions):
        new_faces = []
        midpoint_cache = {}
        
        def get_midpoint(v1, v2):
            key = tuple(sorted([v1, v2]))
            if key in midpoint_cache:
                return midpoint_cache[key]
            p1 = vertices[v1]
            p2 = vertices[v2]
            mx, my, mz = (p1[0]+p2[0])/2, (p1[1]+p2[1])/2, (p1[2]+p2[2])/2
            l = math.sqrt(mx*mx + my*my + mz*mz)
            mx, my, mz = mx/l*radius, my/l*radius, mz/l*radius
            vertices.append((mx, my, mz))
            idx = len(vertices) - 1
            midpoint_cache[key] = idx
            return idx
            
        for face in faces:
            v1, v2, v3 = face
            a = get_midpoint(v1, v2)
            b = get_midpoint(v2, v3)
            c = get_midpoint(v3, v1)
            
            new_faces.append((v1, a, c))
            new_faces.append((v2, b, a))
            new_faces.append((v3, c, b))
            new_faces.append((a, b, c))
        faces = new_faces
        
    return vertices, faces

vertices, faces = generate_icosphere(1.0, 2)
with open("c:/Users/k024g/OneDrive/デスクトップ/2年/2年前期/CG2/CG2/resources/sphere.obj", "w") as f:
    for v in vertices:
        f.write(f"v {v[0]:.4f} {v[1]:.4f} {v[2]:.4f}\n")
    for face in faces:
        f.write(f"f {face[0]+1} {face[1]+1} {face[2]+1}\n")
print('sphere.obj generated!')
