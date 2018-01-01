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

(function() { function a(b){for(var e=d,f=[],c=0;c<b;c++)f.push(g(e));return f}function g(b){var e=h+l++;return m(8,b,e)}function n(b){return m(4,b,void 0)}function m(b,e,f){var c={},k;for(k in e)c[k]=e[k];c.type=b;f&&(c.id=f);return{spec:c}};var l=0,h="";var h="101kbd-k-",p;p=n({id:"topBar",children:[m(10,{id:"candidateView",widthInWeight:15,heightInWeight:1},void 0)],condition:"showTitleBar"});var q,d={widthInWeight:1,heightInWeight:1},r=a(13),s=g({widthInWeight:2}),t=n({id:"row1",children:[r,s]}),u=g({widthInWeight:1.5}),v=a(12),w=g({widthInWeight:1.5}),x=n({id:"row2",children:[u,v,w]}),y=g({widthInWeight:1.75}),z=a(11),A=g({widthInWeight:2.25}),B=n({id:"row3",children:[y,z,A]}),C=g({widthInWeight:2.5}),D=a(10),E=g({widthInWeight:2.5});
q=[t,x,B,n({id:"row4",children:[C,D,E]})];var F=g({widthInWeight:1}),G=g({widthInWeight:1}),H=g({widthInWeight:1,condition:"showCompactLayoutSwitcher"}),I=g({widthInWeight:8}),J=g({widthInWeight:1,condition:"showAltGr"});H.spec.giveWeightTo=I.spec.id;J.spec.giveWeightTo=I.spec.id;var K=g({widthInWeight:1}),L=g({widthInWeight:1}),M=g({widthInWeight:1});
google.ime.chrome.inputview.onLayoutLoaded({layoutID:"101kbd",heightPercentOfWidth:.3,minimumHeight:380,fullHeightInWeight:6,children:[p,n({id:"keyboardContainer",children:[m(3,{id:"keyboardView",children:[q,n({id:"spaceKeyrow",children:[F,G,H,I,J,K,L,M]})],widthPercent:100,heightPercent:100},void 0)]})]}); })()
