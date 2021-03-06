// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The encodeURI property has not prototype property
es5id: 15.1.3.3_A5.6
es6id: 18.2.6.4
esid: sec-encodeuri-uri
description: Checking encodeURI.prototype
---*/

//CHECK#1
if (encodeURI.prototype !== undefined) {
  $ERROR('#1: encodeURI.prototype === undefined. Actual: ' + (encodeURI.prototype));
}

reportCompare(0, 0);
