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

(function() { function f(d,a){return d.type=a}
var g="push",h="chrome",k="inputview",l="length",n="onConfigLoaded",p=" ",q="?123",r="ArrowDown",t="ArrowLeft",u="ArrowRight",w="ArrowUp",x="Backspace",y="Enter",z="HideKeyboard",A="ShiftLeft",B="ShiftRight",C="Space",D="abc",aa="compactkbd",ba="compactkbd-k-",E="compactkbd-k-key-",ca="inputview-arrow-key ",da="inputview-backspace-icon",ea="inputview-down-key",fa="inputview-enter-icon",ga="inputview-hide-keyboard-icon",ha="inputview-left-key",ia="inputview-right-key",ja="inputview-shift-icon",ka=
"inputview-up-key",F="us.compact",G="us.compact.123",H="us.compact.more",I="~[<";function J(d,a,b,v){K.splice.apply(d,la(arguments,1))}function la(d,a,b){return 2>=arguments[l]?K.slice.call(d,a):K.slice.call(d,a,b)}function L(){return M({iconCssClass:ga,type:19,id:z})}function N(d,a){var b={toState:1,iconCssClass:ja,type:5};b.id=d?A:B;b.supportSticky=a;return M(b)}function P(){return M({iconCssClass:fa,type:12,id:y})}function Q(){return M({iconCssClass:da,type:13,id:x})}
function R(){return M({name:p,type:11,id:C})}function S(d){var a={iconCssClass:ca};0==d?(a.id=w,a.iconCssClass+=ka,f(a,15)):1==d?(a.id=r,a.iconCssClass+=ea,f(a,16)):2==d?(a.id=t,a.iconCssClass+=ha,f(a,17)):3==d&&(a.id=u,a.iconCssClass+=ia,f(a,18));return M(a)}function M(d){var a={},b;for(b in d)a[b]=d[b];return{spec:a}}
function T(d,a){for(var b=[],v={},e=0;e<d[l]-1;e++){var c;c=d[e][0];var m=2==d[e][l]?d[e][1]:void 0,O=10==e?.33:0,ma=3==d[e][l]?!!d[e][2]:!1,s={};s.id=E+e;f(s,22);s.text=c;m&&(s.hintText=m);O&&(s.marginLeftPercent=O);s.isGrey=!!ma;c=M(s);b[g](c)}e=R();c=b[l];J(b,10,0,Q());J(b,20,0,P());switch(a){case 0:J(b,21,0,N(!0,!0));J(b,31,0,N(!1,!0));c=U(E+c++,q,G);J(b,32,0,c);break;case 1:m=U(E+c++,I,H);J(b,21,0,m);m=U(E+c++,I,H);J(b,31,0,m);c=U(E+c++,D,F);J(b,32,0,c);break;case 2:m=U(E+c++,q,G),J(b,21,0,m),
m=U(E+c++,q,G),J(b,31,0,m),c=U(E+c++,D,F),J(b,32,0,c)}J(b,35,0,e);b[g](L());for(e=0;e<b[l];e++)c=b[e],v[c.spec.id]=ba+e;return{keyList:b,mapping:v,layout:aa}}function U(d,a,b,v,e){var c={};c.id=d;c.name=a;c.toKeyset=b;c.iconCssClass=v;f(c,21);c.record=!!e;return M(c)};var K=Array.prototype;for(var V,W=[["`","~"],["1","!"],["2","@"],["3","#"],["4","$"],["5","%"],["6","^"],["7","&"],["8","*"],["9","("],["0",")"],["-","_"],["=","+"],["q","Q"],["w","W"],["e","E"],["r","R"],["t","T"],["y","Y"],["u","U"],["i","I"],["o","O"],["p","P"],["[","{"],["]","}"],["\\","|"],["a","A"],["s","S"],["d","D"],["f","F"],["g","G"],["h","H"],["j","J"],["k","K"],["l","L"],[";",":"],["'",'"'],["z","Z"],["x","X"],["c","C"],["v","V"],["b","B"],["n","N"],["m","M"],[",","<"],[".",">"],["/","?"],[p,p]],X=[],Y={},
na="Backquote Digit1 Digit2 Digit3 Digit4 Digit5 Digit6 Digit7 Digit8 Digit9 Digit0 Minus Equal KeyQ KeyW KeyE KeyR KeyT KeyY KeyU KeyI KeyO KeyP BracketLeft BracketRight Backslash KeyA KeyS KeyD KeyF KeyG KeyH KeyJ KeyK KeyL Semicolon Quote KeyZ KeyX KeyC KeyV KeyB KeyN KeyM Comma Period Slash".split(" "),Z=0;Z<W[l]-1;Z++){var $=M({id:na[Z],type:6,characters:W[Z]});X[g]($)}var oa=R();J(X,13,0,Q());J(X,14,0,M({iconCssClass:"inputview-tab-icon",type:14,id:"Tab"}));
J(X,28,0,M({toState:4,name:"caps",type:5,id:"OsLeft"}));J(X,40,0,P());J(X,41,0,N(!0));X[g](N(!1));X[g](M({toState:8,name:"ctrl",type:5,id:"ControlLeft"}));X[g](M({toState:16,name:"alt",type:5,id:"AltLeft"}));X[g](U("toCompact","",F,"inputview-compact-switcher",!0));X[g](oa);X[g](M({toState:2,name:"altgr",type:5,id:"AltRight"}));X[g](S(2));X[g](S(3));X[g](L());for(Z=0;Z<X[l];Z++)$=X[Z],Y[$.spec.id]="101kbd-k-"+Z;V={keyList:X,mapping:Y,layout:"101kbd",hasAltGrKey:!1,id:"us"};google.ime[h][k][n](V);
V=T([["q","1"],["w","2"],["e","3"],["r","4"],["t","5"],["y","6"],["u","7"],["i","8"],["o","9"],["p","0"],["a"],["s"],["d"],["f"],["g"],["h"],["j"],["k"],["l"],["z"],["x"],["c"],["v"],["b"],["n"],["m"],["!"],["?"],["/"],["-"],[","],["."],[p]],0);V.id=F;google.ime[h][k][n](V);V=T([["1"],["2"],["3"],["4"],["5"],["6"],["7"],["8"],["9"],["0"],["@"],["#"],["$"],["%"],["&"],["-"],["+"],["("],[")"],["\\"],["="],["*"],['"'],["'"],[":"],[";"],["!"],["?"],["/"],["_"],[","],["."],[p]],1);V.id=G;google.ime[h][k][n](V);
V=T([["~"],["`"],["|"],["\u2022"],["\u23b7"],["\u03a0"],["\u00f7"],["\u00d7"],["\u00b6"],["\u0394"],["\u00a3"],["\u00a2"],["\u20ac"],["\u00a5"],["^"],["\u00b0"],["="],["{"],["}"],["\\"],["\u00a9"],["\u00ae"],["\u2122"],["\u2105"],["["],["]"],["\u00a1"],["\u00bf"],["<"],[">"],[","],["."],[p]],2);V.id=H;google.ime[h][k][n](V); })()
