import random

def str_random (n):
    c = ""
    x = int (random.random() * 100)
    y = int (random.random() * 100)
    z = int (random.random() * 100)

    for i in xrange(n):
        x = (171 * x) % 30269
        y = (172 * y) % 30307
        z = (170 * z) % 30323
        c += chr(32 + int((x/30269.0 + y/30307.0 + z/30323.0) * 1000 % 95))

    return c

def letters_random (n):
    c = ""
    x = int (random.random() * 100)
    y = int (random.random() * 100)
    z = int (random.random() * 100)

    for i in xrange(n):
        x = (171 * x) % 30269
        y = (172 * y) % 30307
        z = (170 * z) % 30323
        if z%2:
            c += chr(65 + int((x/30269.0 + y/30307.0 + z/30323.0) * 1000 % 25))
        else:
            c += chr(97 + int((x/30269.0 + y/30307.0 + z/30323.0) * 1000 % 25))
            
    return c
