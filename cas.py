#!/usr/bin/env python

import bottle
import cgi
import hashlib
import hmac
import os

cgi.maxlen = 1048576
root = '/tmp/cas-store'
key = 'secret-hmac-key'
me = 'http://127.0.0.1:8080/'
host = '127.0.0.1'
port = 8080

try:
    os.stat(root)
except OSError:
    os.mkdir(root, 0700)

def digest(r):
    return hashlib.sha256(r).hexdigest()

def mac(r):
    if not key:
        return ''
    return hmac.HMAC(key, msg=r, digestmod=hashlib.sha256).hexdigest()

@bottle.get('/:path')
def get(path):
    return bottle.static_file(path, root=root)

@bottle.post('/post/:fmac')
def post(fmac):
    d = bottle.request.files
    if not d or 'file' not in d:
        bottle.abort(400, 'Need a file')
    r = d['file'].file.read()
    v = digest(r)
    m = mac(v)
    if key and digest(m) != digest(fmac):
        bottle.abort(400, 'HMAC failure')
    with open(root + '/' + v, 'w') as f0:
        f0.write(r)
    return me + v + '\n'

bottle.debug()
bottle.run(host=host, port=port)
