// |reftest| error:SyntaxError
// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    `yield` expressions are not LogicalOrExpressions.
es6id: 12.1.1
negative:
  phase: parse
  type: SyntaxError
features: [generators]
---*/

throw "Test262: This statement should not be evaluated.";


var g = function*() {
  yield ? yield : yield;
};
