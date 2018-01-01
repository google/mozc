// Copyright 2010-2018, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

(function() { function a(b){for(var d=e,f=[],c=0;c<b;c++)f.push(g(d));return f}function g(b){var d=h+l++;return m(8,b,d)}function n(b){return m(4,b,void 0)}function m(b,d,f){var c={},k;for(k in d)c[k]=d[k];c.type=b;f&&(c.id=f);return{spec:c}};var l=0,h="";var h="compactkbd-k-",p;p=n({id:"topBar",children:[m(10,{id:"candidateView",widthInWeight:11.2,heightInWeight:.6,numberRowWeight:10},void 0)],condition:"showTitleBar"});
var e={widthInWeight:1,heightInWeight:1},q=a(10),r=g({widthInWeight:1.2}),s=n({id:"row1",children:[q,r]}),t=g({widthInWeight:1.5}),u=a(8),v=g({widthInWeight:1.7}),w=n({id:"row2",children:[t,u,v]}),x=g({widthInWeight:1.1}),y=a(9),z=g({widthInWeight:1.1}),A=n({id:"row3",children:[x,y,z]}),B=g({widthInWeight:1.1}),C=g({widthInWeight:1}),D=g({widthInWeight:1}),E=g({widthInWeight:5}),F=g({widthInWeight:1}),G=g({widthInWeight:1}),H=g({widthInWeight:1.1});
google.ime.chrome.inputview.onLayoutLoaded({layoutID:"compactkbd",heightPercentOfWidth:.3,minimumHeight:380,fullHeightInWeight:4.6,children:[p,n({id:"keyboardContainer",children:[m(3,{id:"keyboardView",children:[[s,w,A,n({id:"row4",children:[B,C,D,E,F,G,H]})]],widthPercent:100,heightPercent:100},void 0)]})]}); })()
