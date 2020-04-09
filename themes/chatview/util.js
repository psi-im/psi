function initPsiTheme() {
    var server = window.srvUtil;
    var loader = window.srvLoader;
    var session = window.srvSession;
    server.console("Util is initilizing");
    var htmlSource = document.createElement("div"); //manages appendHtml
    var async = (typeof QWebChannel != 'undefined');
    var usersMap = {};
    var nextServerTransaction = 0;
    var serverTransctions = {};
    var uniqReplId = Number(0);
    var previewsEnabled = true;
    var optionChangeHandlers = {}

    function BackForthScollerPausedAnimation(start, stop, callback)
    {
        callback(start);
        var that = this;
        var startTime = performance.now();
        this.id = setInterval(function animate() {
            requestAnimationFrame(function(time){
                if (!that.id) return;
                var angle = (time - startTime) / 5000;
                var dist = stop - start;
                callback(start + (1 - Math.cos(angle)) / 2 * dist);
            });
        }, 70); // we can just requestAnimationFrame but this raises cpu load twice

        this.stop = function() { if (this.id){clearInterval(this.id); this.id = null;}};
    }

    function AudioMessage(el)
    {
        var playing = false;
        var audio = el.querySelector("audio");
        var progressBar = el.querySelector("progress");
        var titleAnim = null;
        var detectedDuration = 0;

        function updateTitleScroller() {
            if (titleAnim) titleAnim.stop();

            var info = el.querySelector(".psi-am-info > div")
            if (!info) return;

            if (playing) {
                if (info.scrollWidth > info.clientWidth) {
                    titleAnim = new PsiBackForthScollerPausedAnimation(0, info.scrollWidth - info.clientWidth, function(x){
                        info.scrollLeft = x;
                    });
                }
            } else {
                info.scrollLeft = 0;
                titleAnim = null;
            }
        }

        function markStopped() {
            var sign = el.querySelector(".psi-am-play-sign");
            sign.className = sign.className.replace(/\bpsi-am-sign-stop\b/, "");
            sign.className += " psi-am-sign-play";
            playing = false;
            updateTitleScroller();
        }

        var that = {
            play: function() {
                if (playing) return;
                var sign = el.querySelector(".psi-am-play-sign");
                sign.className = sign.className.replace(/\bpsi-am-sign-play\b/, "");
                sign.className += " psi-am-sign-stop";
                playing = true;
                audio.play();
                updateTitleScroller();
            },

            stop: function() {
                if (!playing) return;
                markStopped();
                audio.pause();
            },

            seekFraction: function(fraction) {
                audio.currentTime = fraction * detectedDuration;
                progressBar.value = fraction * progressBar.max;
            }

        }

        el.querySelector(".psi-am-play-btn").addEventListener("click", function(event) {
            if (playing) that.stop();
            else that.play();
            event.preventDefault();
        });
        audio.addEventListener("durationchange", function(event) {
            progressBar.max = audio.duration;
            detectedDuration = audio.duration;
            //server.console("duration changed: " + audio.duration + " set progress bar max: " + progressBar.max);
        });
        audio.addEventListener("timeupdate", function(event) {
            if (!isFinite(audio.duration) && audio.currentTime > detectedDuration) {
                //server.console("set max to current. new max=" + progressBar.max);
                detectedDuration = audio.currentTime; 
                progressBar.max = audio.currentTime;
            }
            progressBar.value = audio.currentTime;
            //server.console("time updated: " + audio.currentTime + " set progress bar current value: " + progressBar.value + " duration: " + audio.duration);
        });
        audio.addEventListener("ended", markStopped);
        progressBar.addEventListener("click", function(event) { that.seekFraction(event.offsetX / progressBar.clientWidth) });

        updateTitleScroller();

        return that;
    }

    var chat =  {
        async : async,
        console : server.console,
        server : server,
        session : session,
        hooks: [],

        util: {
            console : server.console,
            showCriticalError : function(text) {
                var e=document.body || document.documentElement.appendChild(document.createElement("body"));
                var er = e.appendChild(document.createElement("div"))
                er.style.cssText = "background-color:red;color:white;border:1px solid black;padding:1em;margin:1em;font-weight:bold";
                er.innerHTML = chat.util.escapeHtml(text).replace(/\n/, "<br/>");
            },

            // just for debug
            escapeHtml : function(html) {
                html += ""; //hack
                return html.split("&").join("&amp;").split( "<").join("&lt;").split(">").join("&gt;");
            },

            // just for debug
            props : function(e, rec) {
                var ret='';
                for (var i in e) {
                    var gotValue = true;
                    var val = null;
                    try {
                        val = e[i];
                    } catch(err) {
                        val = err.toString();
                        gotValue = false;
                    }
                    if (gotValue) {
                        if (val instanceof Object && rec && val.constructor != Date) {
                            ret+=i+" = "+val.constructor.name+"{"+chat.util.props(val, rec)+"}\n";
                        } else {
                            if (val instanceof Function) {
                                ret+=i+" = Function: "+i+"\n";
                            } else {
                                ret+=i+" = "+(val === null?"null\n":val.constructor.name+"(\""+val+"\")\n");
                            }
                        }
                    } else {
                        ret+=i+" = [CAN'T GET VALUE: "+val+"]\n";
                    }
                }
                return ret;
            },

            startSessionTransaction: function(starter, finisher) {
                var tId = "st" + (++nextServerTransaction);
                serverTransctions[tId] = finisher;
                starter(tId);
            },

            _remoteCallEval : function(func, args, cb) {
                function ecb(val) { val = eval("[" + val + "][0]"); cb(val); }

                if (chat.async) {
                    args.push(ecb)
                    func.apply(this, args)
                } else {
                    var val = func.apply(this, args);
                    ecb(val);
                }
            },

            _remoteCall : function(func, args, cb) {
                if (chat.async) {
                    args.push(cb)
                    func.apply(this, args)
                } else {
                    var val = func.apply(this, args);
                    cb(val);
                }
            },

            psiOption : function(option, cb) { chat.util._remoteCallEval(server.psiOption, [option], cb); },
            colorOption : function(option, cb) { chat.util._remoteCallEval(server.colorOption, [option], cb); },
            getFont : function(cb) { chat.util._remoteCallEval(session.getFont, [], cb); },
            getPaletteColor : function(name, cb) { chat.util._remoteCall(session.getPaletteColor, [name], cb); },
            connectOptionChange: function(option, cb) {
                if (typeof optionChangeHandlers[option] == 'undefined') {
                    optionChangeHandlers[option] = {value: undefined, handlers:[]};
                }
                optionChangeHandlers[option].handlers.push(cb);
            },
            rereadOptions: function() {
                onOptionsChanged(Object.getOwnPropertyNames(optionChangeHandlers));
            },

            // replaces <icon name="icon_name" text="icon_text" />
            // with <img src="/psi/icon/icon_name" title="icon_text" />
            icon2img : function (obj) {
                var img = document.createElement('img');
                img.src = "/psi/icon/" + obj.getAttribute("name");
                img.title = obj.getAttribute("text");
                obj.parentNode.replaceChild(img, obj);
            },

            // replaces all occurrence of <icon> by function above
            replaceIcons : function(el) {
                var els = el.querySelectorAll("icon"); // frozen list
                for (var i=0; i < els.length; i++) {
                    chat.util.icon2img(els[i]);
                }
            },

            replaceBob : function(el) {
                var els = el.querySelectorAll("img"); // frozen list
                for (var i=0; i < els.length; i++) {
                    if (els[i].src.indexOf('cid:') == 0) {
                        els[i].src = "/psibob/" + els[i].src.slice(4);
                    }
                }
            },

            updateObject : function(object, update) {
                for (var i in update) {
                    object[i] = update[i]
                }
            },

            findStyleSheet : function (sheet, selector) {
                for (var i=0; i<sheet.cssRules.length; i++) {
                    if (sheet.cssRules[i].selectorText == selector)
                        return sheet.cssRules[i];
                }
                return false;
            },

            createHtmlNode : function(html, context) {
                var range = document.createRange();
                range.selectNode(context || document.body);
                return range.createContextualFragment(html);
            },

            replaceYoutube : function(linkEl) {
                var baseLink = "https://www.youtube.com/embed/";
                var link;

                if (linkEl.hostname == "youtu.be") {
                    link = baseLink + linkEl.pathname.slice(1);
                } else if (linkEl.pathname.indexOf("/embed/") != 0) {
                    var m = linkEl.href.match(/^.*[?&]v=([a-zA-Z0-9_-]+).*$/);
                    var code = m && m[1];
                    if (code) {
                        link = baseLink + code;
                    }
                } else {
                    link = linkEl.href;
                }

                if (link) {
                    var iframe = chat.util.createHtmlNode('<div class="youtube preview"><iframe width="560" height="315" src="'+ link +
                                                          '" frameborder="0" allowfullscreen="1"></iframe></div>');
                    linkEl.parentNode.insertBefore(iframe, linkEl.nextSibling);
                }
            },

            replaceImage : function(linkEl)
            {
                var img = chat.util.createHtmlNode('<div class="image preview"><img style="display:block;max-width:560px; max-height:315px" '+
                                                   'src="'+ linkEl.href +'" border="0">');
                linkEl.parentNode.insertBefore(img, linkEl.nextSibling);
            },

            replaceAudio : function(linkEl)
            {
                var audio = chat.util.createHtmlNode('<div class="audio preview"><audio controls="1"><source src="'+ linkEl.href +'"></audio></div>');
                linkEl.parentNode.insertBefore(audio, linkEl.nextSibling);
            },

            replaceVideo : function(linkEl)
            {
                var audio = chat.util.createHtmlNode('<div class="video preview"><video width="560" height="315" controls="1"><source src="'+ linkEl.href +'"></video></div>');
                linkEl.parentNode.insertBefore(audio, linkEl.nextSibling);
            },

            replaceLinkAsync : function(linkEl)
            {
                chat.util.startSessionTransaction(function(tId) {
                    session.getUrlHeaders(tId, linkEl.href);
                },function(result) {
                    //chat.console("result ready " + chat.util.props(result, true));
                    var ct = result['content-type'];
                    if ((typeof(ct) == "string") && (ct != "application/octet-stream")) {
                        ct = ct.split("/")[0].trim();
                        switch (ct) {
                        case "image":
                            chat.util.replaceImage(linkEl);
                            break;
                        case "audio":
                            chat.util.replaceAudio(linkEl);
                            break;
                        case "video":
                            chat.util.replaceVideo(linkEl);
                            break;
                        }
                    } else { // fallback when no content type
                        //chat.console("fallback")
                        var imageExts = ["png", "jpg", "jpeg", "gif", "webp"];
                        var audioExts = ["mp3", "ogg", "aac", "flac", "wav", "m4a"];
                        var videoExts = ["mp4", "webm", "mkv", "mov", "avi", "ogv"];
                        var lpath = linkEl.pathname.toLowerCase().split('#')[0].split('?')[0];
                        function checkExt(exts, replacer) {
                            for (var i = 0; i < exts.length; i++) {
                                if (lpath.slice(lpath.length - exts[i].length - 1) == ("." + exts[i])) {
                                    replacer(linkEl);
                                    break;
                                }
                            }
                        }
                        checkExt(imageExts, chat.util.replaceImage);
                        checkExt(audioExts, chat.util.replaceAudio);
                        checkExt(videoExts, chat.util.replaceVideo);
                    }
                });
            },

            handleLinks : function(el)
            {
                if (!previewsEnabled)
                    return;
                var links = el.querySelectorAll("a");
                var youtube = ["youtu.be", "www.youtube.com", "youtube.com", "m.youtube.com"];
                for (var li = 0; li < links.length; li++) {
                    var linkEl = links[li];
                    if (youtube.indexOf(linkEl.hostname) != -1) {
                        chat.util.replaceYoutube(linkEl);
                    } else if ((linkEl.protocol == "http:" || linkEl.protocol == "https:" || linkEl.protocol == "file:") && linkEl.hostname != "psi") {
                        chat.util.replaceLinkAsync(linkEl);
                    }
                }
            },

            handleShares : function(el) {
                var shares = el.querySelectorAll("share");
                for (var li = 0; li < shares.length; li++) {
                    var share = shares[li];
                    var info = ""; // TODO
                    var source = share.getAttribute("id");
                    var type = share.getAttribute("type");
                    if (type.startsWith("audio")) {
                        var hg = share.getAttribute("amplitudes");
                        if (hg && hg.length)
                            hg.split(",").forEach(v => { info += `<b style="height:${v}%"></b>` });
                        var playerFragment = chat.util.createHtmlNode(`<div class="psi-audio-msg">
  <div class="psi-am-play-btn"><div class="psi-am-play-sign psi-am-sign-play"></div></div>
  <div class="psi-am-info">
  <div>
  ${info}
  </div>
  </div>
  <progress class="psi-am-progressbar"/>
  <audio>
    <source src="/psi/account/${session.account}/sharedfile/${source}" type="${type}">
  </audio>
</div>`);
                        var player = playerFragment.firstChild;
                        if (share.nextSibling)
                            share.parentNode.insertBefore(playerFragment, share.nextSibling);
                        else
                            share.parentNode.appendChild(playerFragment);
                        new AudioMessage(player);
                    }
                    else if (type.startsWith("image")) {
                        let img = chat.util.createHtmlNode(`<div class="image preview"><img src="/psi/account/${session.account}/sharedfile/${source}"></div>`);
                        if (share.nextSibling)
                            share.parentNode.insertBefore(img, share.nextSibling);
                        else
                            share.parentNode.appendChild(img);
                    }
                }
            },

            prepareContents : function(html) {
                htmlSource.innerHTML = html;
                chat.util.replaceBob(htmlSource);
                chat.util.handleLinks(htmlSource);
                chat.util.replaceIcons(htmlSource);
                chat.util.handleShares(htmlSource);
            },

            appendHtml : function(dest, html) {
                chat.util.prepareContents(html);
                while (htmlSource.firstChild) dest.appendChild(htmlSource.firstChild);
            },

            siblingHtml : function(dest, html) {
                chat.util.prepareContents(html);
                while (htmlSource.firstChild) dest.parentNode.insertBefore(htmlSource.firstChild, dest);
            },

            ensureDeleted : function(id) {
                if (id) {
                    var el = document.getElementById(id);
                    if (el) {
                        el.parentNode.removeChild(el);
                    }
                }
            },

            loadXML : function(path, callback) {
                function cb(text){
                    if (!text) {
                        throw new Error("File " + path + " is empty. can't parse xml");
                    }
                    var xml;
                    try {
                        xml = new DOMParser().parseFromString(text, "text/xml");
                    } catch (e) {
                        server.console("failed to parse xml from file " + path);
                        throw e;
                    }
                    callback(xml);
                }
                if (chat.async) {
                    //server.console("loading xml async: " + path);
                    loader.getFileContents(path, cb);
                } else {
                    //server.console("loading xml sync: " + path);
                    cb(loader.getFileContents(path));
                }
            },

            dateFormat : function(val, format) {
                return (new chat.DateTimeFormatter(format)).format(val);
            },

            avatarForNick : function(nick) {
                var u = usersMap[nick];
                return u && u.avatar;
            },

            nickColor : function(nick) {
                var u = usersMap[nick];
                return u && u.nickcolor;
            },

            replaceableMessage : function(isMuc, isLocal, nick, msgId, text) {
                // if we have an id then this is a replacable message.
                // next weird logic is as follows:
                //   - wrapping to some element may break elements flow
                //   - using well know elements may break styles
                //   - setting just starting mark is useless (we will never find correct end)
                var uniqId;
                if (isMuc) {
                    var u = usersMap[nick];
                    if (!u) {
                        return text;
                    }

                    uniqId = "pmr"+uniqReplId.toString(36); // pmr - psi message replace :-)
                    //chat.console("Sender:"+nick);
                    usersMap[nick].msgs[msgId] = uniqId;
                } else {
                    var uId = isLocal?"l":"r";
                    uniqId = "pmr"+uId+uniqReplId.toString(36);
                    if (!usersMap[uId]) {
                        usersMap[uId]={msgs:{}};
                    }
                    usersMap[uId].msgs[msgId] = uniqId;
                }

                uniqReplId++;
                // TODO better remember elements themselves instead of some id.
                return "<psims mid=\"" + uniqId + "\"></psims>" + text + "<psime mid=\"" + uniqId + "\"></psime>";
            },

            replaceMessage : function(parentEl, isMuc, isLocal, nick, msgId, newId, text) {
                var u
                if (isMuc) {
                    u = usersMap[nick];
                } else {
                    u = usersMap[isLocal?"l":"r"];
                }
                //chat.console(isMuc + " " + isLocal + " " + nick + " " + msgId + " " + chat.util.props(u, true));

                var uniqId = u && u.msgs[msgId];
                if (!uniqId)
                    return false; // replacing something we didn't use replaceableMessage for? hm.

                var se =parentEl.querySelector("psims[mid='"+uniqId+"']");
                var ee =parentEl.querySelector("psime[mid='"+uniqId+"']");
//                chat.console("Replace: start: " + (se? "found, ":"not found, ") +
//                             "end: " + (ee? "found, ":"not found, ") +
//                             "parent match: " + ((se && ee && se.parentNode === ee.parentNode)?"yes":"no"));
                if (se && ee && se.parentNode === ee.parentNode) {
                    delete u.msgs[msgId]; // no longer used. will be replaced with newId.
                    while (se.nextSibling !== ee) {
                        se.parentNode.removeChild(se.nextSibling);
                    }
                    var node = chat.util.createHtmlNode(chat.util.replaceableMessage(isMuc, isLocal, nick, newId, text + "<img src=\"/psi/icon/psi/action_templates_edit\">"));
                    //chat.console(chat.util.props(node));
                    chat.util.handleLinks(node);
                    chat.util.replaceIcons(node);
                    ee.parentNode.insertBefore(node, ee);
                    return true;
                }
                return false;
            }
        },

        WindowScroller : function(animate) {
            var o=this, state, timerId
            var ignoreNextScroll = false;
            o.animate = animate;
            o.atBottom = true; //just a state of aspiration

            var animationStep = function() {
                timerId = null;
                var before = document.body.clientHeight - (window.innerHeight+window.pageYOffset);
                var step = before;
                if (o.animate) {
                    step = step>200?200:(step<8?step:Math.floor(step/1.7));
                }
                ignoreNextScroll = true;
                window.scrollTo(0, document.body.clientHeight - window.innerHeight - before + step);
                if (before>0) {
                    timerId = setTimeout(animationStep, 70); //next step in 250ms even if we are already at bottom (control shot)
                }
            }

            var startAnimation = function() {
                if (timerId) return;
                if (document.body.clientHeight > window.innerHeight) { //if we have what to scroll
                    timerId = setTimeout(animationStep, 0);
                }
            }

            var stopAnimation = function() {
                if (timerId) {
                    clearTimeout(timerId);
                    timerId = null;
                }
            }

            // ensure we at bottom on window resize
            if (typeof ResizeObserver === 'undefined') {

                // next code is copied from www.backalleycoder.com/2013/03/18/cross-browser-event-based-element-resize-detection/ on 7 Dec 2018
                (function(){
                  var attachEvent = document.attachEvent;
                  var isIE = navigator.userAgent.match(/Trident/);
                  //console.log(isIE);
                  var requestFrame = (function(){
                    var raf = window.requestAnimationFrame || window.mozRequestAnimationFrame || window.webkitRequestAnimationFrame ||
                        function(fn){ return window.setTimeout(fn, 20); };
                    return function(fn){ return raf(fn); };
                  })();

                  var cancelFrame = (function(){
                    var cancel = window.cancelAnimationFrame || window.mozCancelAnimationFrame || window.webkitCancelAnimationFrame ||
                           window.clearTimeout;
                    return function(id){ return cancel(id); };
                  })();

                  function resizeListener(e){
                    var win = e.target || e.srcElement;
                    if (win.__resizeRAF__) cancelFrame(win.__resizeRAF__);
                    win.__resizeRAF__ = requestFrame(function(){
                      var trigger = win.__resizeTrigger__;
                      trigger.__resizeListeners__.forEach(function(fn){
                        fn.call(trigger, e);
                      });
                    });
                  }

                  function objectLoad(e){
                    this.contentDocument.defaultView.__resizeTrigger__ = this.__resizeElement__;
                    this.contentDocument.defaultView.addEventListener('resize', resizeListener);
                  }

                  window.addResizeListener = function(element, fn){
                    if (!element.__resizeListeners__) {
                      element.__resizeListeners__ = [];
                      if (attachEvent) {
                        element.__resizeTrigger__ = element;
                        element.attachEvent('onresize', resizeListener);
                      }
                      else {
                        if (getComputedStyle(element).position == 'static') element.style.position = 'relative';
                        var obj = element.__resizeTrigger__ = document.createElement('object');
                        obj.setAttribute('style', 'display: block; position: absolute; top: 0; left: 0; height: 100%; width: 100%; overflow: hidden; pointer-events: none; z-index: -1;');
                        obj.__resizeElement__ = element;
                        obj.onload = objectLoad;
                        obj.type = 'text/html';
                        if (isIE) element.appendChild(obj);
                        obj.data = 'about:blank';
                        if (!isIE) element.appendChild(obj);
                      }
                    }
                    element.__resizeListeners__.push(fn);
                  };

                  window.removeResizeListener = function(element, fn){
                    element.__resizeListeners__.splice(element.__resizeListeners__.indexOf(fn), 1);
                    if (!element.__resizeListeners__.length) {
                      if (attachEvent) element.detachEvent('onresize', resizeListener);
                      else {
                        element.__resizeTrigger__.contentDocument.defaultView.removeEventListener('resize', resizeListener);
                        element.__resizeTrigger__ = !element.removeChild(element.__resizeTrigger__);
                      }
                    }
                  }
                })();
                // end of copied code
                addResizeListener(document.body, function(){
                    o.invalidate();
                });
            } else {
                const ro = new ResizeObserver(function(entries) {
                    o.invalidate();
                });

                // Observe the scrollingElement for when the window gets resized
                ro.observe(document.scrollingElement);
                // Observe the timeline to process new messages
                // ro.observe(timeline);
                
            }

            //let's consider scroll may happen only by user action
            window.addEventListener("scroll", function(){
                if (ignoreNextScroll) {
                    ignoreNextScroll = false;
                    return;
                }
                stopAnimation();
                o.atBottom = document.body.clientHeight == (window.innerHeight+window.pageYOffset);
            }, false);

            //EXTERNAL API
            // checks current state of scroll and wish and activates necessary actions
            o.invalidate = function() {
                if (o.atBottom) {
                    startAnimation();
                }
            }

            o.force = function() {
                o.atBottom = true;
                o.invalidate();
            }
        },

        DateTimeFormatter : function(formatStr) {
            function convertToTr35(format)
            {
                var ret=""
                var i = 0;
                var m = {M: "mm", H: "HH", S: "ss", c: "EEEE', 'MMMM' 'd', 'yyyy' 'G",
                         A: "EEEE", I: "hh", p: "a", Y: "yyyy"}; // if you want me, report it.

                var txtAcc = "";
                while (i < format.length) {
                    var c;
                    if (format[i] === "'" ||
                            (format[i] === "%" && i < (format.length - 1) && (c = m[format[i+1]])))
                    {
                        if (txtAcc) {
                            ret += "'" + txtAcc + "'";
                            txtAcc = "";
                        }
                        if (format[i] === "'") {
                            ret += "''";
                        } else {
                            ret += c;
                            i++;
                        }
                    } else {
                        txtAcc += format[i];
                    }
                    i++;
                }
                if (txtAcc) {
                    ret += "'" + txtAcc + "'";
                    txtAcc = "";
                }
                return ret;
            }

            function convertToMoment(format) {
                var inTxt = false;
                var i;
                var m = {j:"h"}; // sadly "j" is not supported
                var ret = "";
                for (i = 0; i < format.length; i++) {
                    if (format[i] == "'") {
                        ret += (inTxt? ']' : '[');
                        inTxt = !inTxt;
                    } else {
                        var c;
                        if (!inTxt && (c = m[format[i]])) {
                            ret += c;
                        } else {
                            ret += format[i];
                        }
                    }
                }
                if (inTxt) {
                    ret += "]";
                }

                ret = ret.replace("EEEE", "dddd");
                ret = ret.replace("EEE", "ddd");

                return ret;
            }

            formatStr = formatStr || "j:mm";
            if (formatStr.indexOf('%') !== -1) {
                formatStr = convertToTr35(formatStr);
            }

            formatStr = convertToMoment(formatStr);

            this.format = function(val) {
                if (val instanceof String) {
                    val = Date.parse(val);
                }
                return moment(val).format(formatStr); // FIXME we could speedup it by keeping fomatter instances
            }
        },

        AudioMessage : AudioMessage,

        receiveObject : function(data) {
            for(var i=0; i < chat.hooks.length; i++) {
                try {
                    chat.hooks[i](chat, data);
                } catch (e) {
                    chat.console("hook failed");
                }
            }

            if (data.type == "message") {
                if (data.mtype == "join") {
                    usersMap[data.sender] = {avatar:data.avatar, nickcolor:data.nickcolor, msgs:{}};
                    if (data.nopartjoin) return;
                } else if (data.mtype == "part") {
                    delete usersMap[data.sender];
                    if (data.nopartjoin) return;
                } else if (data.mtype == "newnick") {
                    usersMap[data.newnick] = usersMap[data.sender];
                    delete usersMap[data.sender];
                }
            } else if (data.type == "avatar") {
                usersMap[data.sender].avatar = data.avatar;
            } else if (data.type == "tranend") { // end of session transaction (when c++ code is also asynchronous)
                var t = serverTransctions[data.id];
                if (t) {
                    t(data.value);
                    delete serverTransctions[data.id];
                }
                return;
            } else if (data.type == "receivehooks") {
                var hooks = [];
                for (var i = 0; i < data.hooks.length; i++) {
                    try {
                        // same chat object as everywhere
                        let func = new Function("chat, data", data.hooks[i]); /*jshint -W053 */
                        hooks.push(func);
                    } catch(e) {
                        server.console("Failed to evalute receive hook: " + e + "\n" + data.hooks[i]);
                    }
                }
                chat.hooks = hooks;
            } else if (data.type == "js") {
                try {
                    //server.console("An attempt to execute: " + data.js);
                    // same chat object as everywhere
                    let func = new Function("chat", data.js); /*jshint -W053 */
                    func(chat);
                } catch(e) {
                    server.console("Failed to evalute/execute js: " + e + "\n" + data.js);
                }
            }

            chat.adapter.receiveObject(data)
        }
    }

    function onOptionsChanged(changed)
    {
        var options = [];
        for (var i = 0; i < changed.length; i++) {
            if (optionChangeHandlers.hasOwnProperty(changed[i])) {
                options.push(changed[i]);
            }
        }
        chat.util._remoteCallEval(server.psiOptions, [options], function(values) {
            for (var i=0; i < options.length; i++) {
                var optDesc = optionChangeHandlers[options[i]];
                var newValue = values[i];
                if (optDesc && optDesc.value !== newValue) {
                    //server.console("Value of " + options[i] + " is changed to " + values[i]);
                    optDesc.value = newValue;
                    for (var j=0; j < optDesc.handlers.length; j++) {
                        optDesc.handlers[j](newValue);
                    }
                }
            }
        });
    }

    try {
        chat.adapter = window.psiThemeAdapter(chat);
        server.optionsChanged.connect(onOptionsChanged)
        var updateShowPreviews = function(value) { previewsEnabled = value;  }
        chat.util.psiOption("options.ui.chat.show-previews", updateShowPreviews);
        chat.util.connectOptionChange("options.ui.chat.show-previews", updateShowPreviews)
    } catch(e) {
        server.console("Failed to initialize adapter:" + e + "(Line:" + e.line + ")");
        chat.adapter = {
            receiveObject : function(data) {
                chat.util.showCriticalError("Adapter is not loaded. output impossible!\n\nData was:" + chat.util.props(data));
            }
        }
    }

    //window.srvUtil = null; // don't! we need it in adapter
    window.psiThemeAdapter = null;
    window.psiim = chat;

    server.console("Util successfully initialized");
    return chat;
};
