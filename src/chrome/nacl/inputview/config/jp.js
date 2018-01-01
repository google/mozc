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

(function() { function b(c,a){return c.type=a}var d="push",e="ArrowDown",f="ArrowLeft",h="ArrowRight",k="ArrowUp",l="ShiftLeft",m="ShiftRight",n="inputview-arrow-key ",p="inputview-down-key",q="inputview-left-key",r="inputview-right-key",s="inputview-shift-icon",t="inputview-up-key";function u(c,a,g,H){v.splice.apply(c,w(arguments,1))}function w(c,a,g){return 2>=arguments.length?v.slice.call(c,a):v.slice.call(c,a,g)}
function x(c){var a={toState:1,iconCssClass:s,type:5};a.id=c?l:m;a.supportSticky=void 0;return y(a)}function z(c){var a={iconCssClass:n};0==c?(a.id=k,a.iconCssClass+=t,b(a,15)):1==c?(a.id=e,a.iconCssClass+=p,b(a,16)):2==c?(a.id=f,a.iconCssClass+=q,b(a,17)):3==c&&(a.id=h,a.iconCssClass+=r,b(a,18));return y(a)}function y(c){var a={},g;for(g in c)a[g]=c[g];return{spec:a}};var v=Array.prototype;for(var A=[["`","~"],["1","!"],["2",'"'],["3","#"],["4","$"],["5","%"],["6","&"],["7","'"],["8","("],["9",")"],["0","~"],["-","="],["^","~"],["q","Q"],["w","W"],["e","E"],["r","R"],["t","T"],["y","Y"],["u","U"],["i","I"],["o","O"],["p","P"],["@","`"],["[","{"],["]","}"],["a","A"],["s","S"],["d","D"],["f","F"],["g","G"],["h","H"],["j","J"],["k","K"],["l","L"],[";","+"],[":","*"],["z","Z"],["x","X"],["c","C"],["v","V"],["b","B"],["n","N"],["m","M"],[",","<"],[".",">"],["/","?"],[" "," "]],B=[],C={},
D="Backquote Digit1 Digit2 Digit3 Digit4 Digit5 Digit6 Digit7 Digit8 Digit9 Digit0 Minus Equal KeyQ KeyW KeyE KeyR KeyT KeyY KeyU KeyI KeyO KeyP BracketLeft BracketRight Backslash KeyA KeyS KeyD KeyF KeyG KeyH KeyJ KeyK KeyL Semicolon Quote KeyZ KeyX KeyC KeyV KeyB KeyN KeyM Comma Period Slash".split(" "),E=0;E<A.length-1;E++){var F=y({id:D[E],type:6,characters:A[E]});B[d](F)}var G=y({name:" ",type:11,id:"Space"});u(B,13,0,y({iconCssClass:"inputview-backspace-icon",type:13,id:"Backspace"}));
u(B,14,0,y({iconCssClass:"inputview-tab-icon",type:14,id:"Tab"}));u(B,28,0,y({toState:4,name:"caps",type:5,id:"OsLeft"}));u(B,40,0,y({iconCssClass:"inputview-enter-icon",type:12,id:"Enter"}));u(B,41,0,x(!0));B[d](x(!1));B[d](y({toState:8,name:"ctrl",type:5,id:"ControlLeft"}));B[d](y({toState:16,name:"alt",type:5,id:"AltLeft"}));B[d](y({id:"toCompact",name:"",toKeyset:"us.compact",iconCssClass:"inputview-compact-switcher",type:21,record:!0}));B[d](G);B[d](y({toState:2,name:"altgr",type:5,id:"AltRight"}));
B[d](z(2));B[d](z(3));B[d](y({iconCssClass:"inputview-hide-keyboard-icon",type:19,id:"HideKeyboard"}));for(E=0;E<B.length;E++)F=B[E],C[F.spec.id]="101kbd-k-"+E;google.ime.chrome.inputview.onConfigLoaded({keyList:B,mapping:C,layout:"101kbd",hasAltGrKey:!1,id:"jp"}); })()
