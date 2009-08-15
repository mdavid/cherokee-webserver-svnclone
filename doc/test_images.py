#!/usr/bin/env python

# Cherokee Doc: Image checker
#
# Authors:
#      Alvaro Lopez Ortega <alvaro@alobbs.com>
#
# Copyright (C) 2001-2009 Alvaro Lopez Ortega
# This file is distributed under the GPL2 license.

import os
import sys

def get_img_refs():
    img_refs = {}

    for file in filter(lambda x: x.endswith('.txt'), os.listdir('.')):
        for img_line in filter (lambda x: 'image::' in x, open (file, 'r').readlines()):
            filename = img_line.strip()[7:]
            fin = filename.rfind('[')
            if fin > 0:
                filename = filename[:fin]
            img_refs[filename] = None

    return img_refs.keys()

def get_img_files():
    def is_image(file):
        return file.endswith('.jpg') or file.endswith('.jpeg') or file.endswith('.png')

    tmp = ['media/images/%s'%(x) for x in os.listdir('media/images')]
    return filter (is_image, tmp)

def check_images():
    error = False

    img_refs  = get_img_refs()
    img_files = get_img_files()

    for ref in img_refs:
        if not ref in img_files:
            print "ERROR: %s: File not found" % (ref)
            error = True

    for img in img_files:
        if not img in img_refs:
            print "ERROR: %s: Not longer used" % (ref)
            error = True

    return error

if __name__ == "__main__":
    error = check_images()
    sys.exit(int(error))
