// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 9.5.2
description: >
    Throws a TypeError exception if handler is null
---*/

var p = Proxy.revocable({},{});

p.revoke();

assert.throws(TypeError, function() {
    Object.setPrototypeOf(p.proxy, {});
});

reportCompare(0, 0);
