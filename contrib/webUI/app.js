var panel_states =  {
  "unknown":{
    "status_class": "not_ready",
    "icon_class": "icon house with_key",
    "label":"Unknown"
  },
  "armed_away":{
    "status_class":"armed",
    "icon_class":"icon house with_lock",
    "label":"Armed (away)"
  },
  "armed_away_exit":{
    "status_class":"armed",
    "icon_class":"icon house with_walker",
    "label":"Armed (exit&nbsp;now)"
  },
  "armed_home":{
    "status_class":"armed",
    "icon_class":"icon house with_lock",
    "label":"Armed (home)"
  },
  "armed_home_exit":{
    "status_class":"armed",
    "icon_class":"icon house with_walker",
    "label":"Armed (exit&nbsp;now)"
  },
  "armed_night":{
    "status_class":"armed",
    "icon_class":"icon house with_lock",
    "label":"Armed (night)"
  },
  "armed_night_exit":{
    "status_class":"armed",
    "icon_class":"icon house with_walker",
    "label":"Armed (exit&nbsp;now)"
  },
  "alarming":{
    "status_class":"alarming",
    "icon_class":"icon house with_lock",
    "label":"Alarming!"
  },
  "fire":{
    "status_class":"alarming",
    "icon_class":"icon house with_lock",
    "label":"Fire!"
  },
  "ready":{
    "status_class":"ready",
    "icon_class":"icon house with_key",
    "label":"Ready"
  },
  "notready":{
    "status_class":"not_ready",
    "icon_class":"icon house with_key",
    "label":"Not Ready"
  }
}
panel_states.get = function(key) {
  return (panel_states[key] ? panel_states[key] : panel_states["unknown"]);
}
const elem = id => document.getElementById(id);
const divOut = elem("divOut");

var Debugger = function(klass) {
  this.debug = {}
  if (klass.logLevels) {
    for (var m in console)
      if (typeof console[m] == 'function') {
        if (klass.logLevels.includes(m)) {
          this.debug[m] = console[m].bind(window.console, klass.toString()+": ");
        } else {
          this.debug[m] = function(){};
        }
      }
  } else {
    for (var m in console)
      if (typeof console[m] == 'function')
        this.debug[m] = function(){};
  }
  console.log(this.debug);
  return this.debug;
}

class AD2ws {
  constructor() {
      /* logs to enable options: ['trace', 'debug', 'info', 'warn', 'error'] */
      this.logLevels = ['info', 'warn', 'error'];
      this.debug = Debugger(this);
      this.addressMask = 0xffffffff;
      this.connecting = false;
      this.connected = false;
      this.ws = null;
      this.ad2emb_state = null;
      this.mode = "unknown";
  }

  /* update UI from current state */
  updateUI() {
    /* test if state is valid */
    if (!this.ad2emb_state)
      return;

    /* simple state machine for now to set status class */
    if (this.ad2emb_state.ready) {
      this.mode = "ready";
    } else {
      if (this.ad2emb_state.armed_away) {
        // Note: append _exit if exit_now state is set
        this.mode = "armed_away" + (this.ad2emb_state.exit_now ? "_exit" : "");
      } else
      if (this.ad2emb_state.armed_home) {
        // Note: append _exit if exit_now state is set
        this.mode = "armed_home" + (this.ad2emb_state.exit_now ? "_exit" : "");
      } else
      if (this.ad2emb_state.alarm_sounding) {
        this.mode = "alarming";
      } else {
        this.mode = "notready";
      }
    }
    var ps = panel_states.get(this.mode);
    var statusObj = document.getElementById("status");
    var iconObj = document.getElementById("status_icon");
    var textObj = document.getElementById("status_text");
    statusObj.className = ps.status_class;
    iconObj.className = ps.icon_class;
    textObj.innerHTML = ps.label;
  }

  /* FIXME: docs on simple ws request api */
  /* connect WS to AD2 IoT device and stay connected. */
  wsConnect() {
      if (this.ws === null) {
          this.debug.info("Connecting.");
          divOut.innerHTML = "<p>Connecting.</p>";
          this.connecting = true;
          this.ws = new WebSocket("ws://" + document.location.host + "/ad2ws");

          /* FIXME: need send request for update on state */
          this.ws.onopen = e => {
              this.debug.trace("onopen");
              this.connecting = false;
              this.connected = true;
              divOut.innerHTML = "<p>Connected.</p>";

              /* request the current AD2EMB AlarmDecoder state. */
              this.wsSendMessage("!SYNC:"+this.addressMask);
          };
          /* FIXME: needs more cow bell */
          this.ws.onmessage = e => {
              this.debug.trace("onmessage. '" + e.data + "'");
              if (e.data[0] == "{") {
                this.ad2emb_state = JSON.parse(e.data);
              }
              if (e.data[0] == "!") {
                // !PONG:0000000
              }
              this.updateUI();
          }
          /* reconnect on lost connection. */
          this.ws.onclose = e => {
              this.debug.info("onclose.");
              this.ws = null;
              divOut.innerHTML = "<p>Closed.</p>";
              if (this.connecting) {
                this.connecting = false;
                this.debug.info("was connecting.");
              }
              if (this.connected) {
                  this.connected = false;
                  divOut.innerHTML+="<p>Disconnected.</p>";
              }
              setTimeout(function() {
                ad2ws.wsConnect();
              }, 1000);
          }
          /* on errors */
          this.ws.onerror = e => {
              this.debug.error(e);
              this.ws.close();
          }
      }
  }

  /* check the ws connection */
  wsCheck() {
    this.debug.trace("wsCheck");
    if (this.ws !== null) {
      if (this.ws.readyState === WebSocket.OPEN) {
        this.debug.trace("sending ping.");
        this.debug.trace(this.ws);
        this.wsSendMessage("!PING:"+this.addressMask);
      }
    }
  }

  // send message to AD2EMB IoT web socket.
  wsSendMessage(msg) {
      if (this.ws !== null) {
          this.ws.send(msg);
      }
  }
};


let ad2ws = new AD2ws();
ad2ws.wsConnect();
var loadingTimer = setInterval(function(){ad2ws.wsCheck();}, 15000);
