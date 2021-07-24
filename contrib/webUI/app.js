var panel_states =  {
  "unknown":{
    "status_class": "not_ready",
    "icon_class": "icon house with_key",
    "label":"Unknown"
  },
  "armed_away":{
    "status_class":"armed",
    "icon_class":"icon house with_key",
    "label":"Armed (away)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_walker",
    "b2_label": "EXIT"
  },
  "armed_away_exit":{
    "status_class":"armed",
    "icon_class":"icon house with_walker",
    "label":"Armed (exit&nbsp;now)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_lock",
    "b2_label": "DISARM"
  },
  "armed_stay":{
    "status_class":"armed",
    "icon_class":"icon house with_key",
    "label":"Armed (stay)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_walker",
    "b2_label": "EXIT"
  },
  "armed_stay_exit":{
    "status_class":"armed",
    "icon_class":"icon house with_walker",
    "label":"Armed (exit&nbsp;now)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_lock",
    "b2_label": "DISARM"
  },
  "armed_night":{
    "status_class":"armed",
    "icon_class":"icon house with_key",
    "label":"Armed (night)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_walker",
    "b2_label": "EXIT"
  },
  "armed_night_exit":{
    "status_class":"armed",
    "icon_class":"icon house with_walker",
    "label":"Armed (exit&nbsp;now)",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_walker",
    "b2_label": "EXIT"
  },
  "alarming":{
    "status_class":"alarming",
    "icon_class":"icon house with_key",
    "label":"Alarming!",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_lock",
    "b2_label": "DISARM"
  },
  "fire":{
    "status_class":"alarming",
    "icon_class":"icon house with_key",
    "label":"Fire!",
    "b1_icon_class": "icon house with_lock",
    "b1_label": "DISARM",
    "b2_icon_class": "icon house with_lock",
    "b2_label": "DISARM"
  },
  "ready":{
    "status_class":"ready",
    "icon_class":"icon house with_lock",
    "label":"Ready",
    "b1_icon_class": "icon house with_key",
    "b1_label": "AWAY",
    "b2_icon_class": "icon house with_key",
    "b2_label": "STAY"
  },
  "notready":{
    "status_class":"not_ready",
    "icon_class":"icon house with_lock",
    "label":"Not Ready",
    "b1_icon_class": "icon house with_key",
    "b1_label": "AWAY",
    "b2_icon_class": "icon house with_key",
    "b2_label": "STAY"
  }
}
panel_states.get = function(key) {
  return (panel_states[key] ? panel_states[key] : panel_states["unknown"]);
}
const elem = id => document.getElementById(id);
const divOut = elem("divOut");

 var Debugger = function(klass) {
  this.debug = {}
  if (klass.isDebug) {
    for (var m in console)
      if (typeof console[m] == "function")
        this.debug[m] = console[m].bind(window.console, klass.toString()+": ");
  }else{
    for (var m in console)
      if (typeof console[m] == "function")
        this.debug[m] = function(){}
  }
  return this.debug;
}

class AD2ws {
  constructor() {
      this.isDebug = true;
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
      if (this.ad2emb_state.armed_stay) {
        // Note: append _exit if exit_now state is set
        this.mode = "armed_stay" + (this.ad2emb_state.exit_now ? "_exit" : "");
      } else
      if (this.ad2emb_state.alarm_sounding) {
        this.mode = "alarming";
      } else {
        this.mode = "notready";
      }
    }
    var ps = panel_states.get(this.mode);
    document.getElementById("status").className = ps.status_class;
    document.getElementById("status_icon").className = ps.icon_class;
    document.getElementById("status_text").innerHTML = ps.label;
    document.getElementById("b1_icon").className = ps.b1_icon_class;
    document.getElementById("b1_text").innerHTML = ps.b1_label;
    document.getElementById("b2_icon").className = ps.b2_icon_class;
    document.getElementById("b2_text").innerHTML = ps.b2_label;
  }

  /* FIXME: docs on simple ws request api */
  /* connect WS to AD2 IoT device and stay connected. */
  wsConnect() {
      if (this.ws === null) {
          var wsHost = document.location.host;
          this.debug.info("Connecting.");
          divOut.innerHTML = "<p>Connecting.</p>";
          this.connecting = true;
          this.ws = new WebSocket("ws://" + wsHost + "/ad2ws");

          /* FIXME: need send request for update on state */
          this.ws.onopen = e => {
              this.debug.info("onopen");
              this.connecting = false;
              this.connected = true;
              divOut.innerHTML = "<p>Connected.</p>";

              /* request the current AD2EMB AlarmDecoder state. */
              this.wsSendMessage("!SYNC:"+this.addressMask);
          };
          /* FIXME: needs more cow bell */
          this.ws.onmessage = e => {
              this.debug.info("onmessage. '" + e.data + "'");
              if (e.data[0] == "{") {
                this.ad2emb_state = JSON.parse(e.data);
              }
              if (e.data[0] == "!") {
                this.debug.info("received PONG host is alive.");
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
    this.debug.info("wsCheck");
    if (this.ws !== null) {
      if (this.ws.readyState === WebSocket.OPEN) {
        this.debug.info("sending ping.");
        this.debug.info(this.ws);
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

document.getElementById("b1Href").onclick = function() {
  debug.info("button B1 pressed");
};

document.getElementById("b2Href").onclick = function() {
  debug.info("button B2 pressed");
};

document.getElementById("panicHref").onclick = function() {
  debug.info("button panic panic alarm pressed");
};

document.getElementById("fireHref").onclick = function() {
  debug.info("button fire alarm pressed");
};

document.getElementById("auxHref").onclick = function() {
  debug.info("button aux alarm pressed");
};

document.getElementById("refreshHref").onclick = function() {
  debug.info("button refresh pressed");
};

let ad2ws = new AD2ws();
ad2ws.wsConnect();
var loadingTimer = setInterval(function(){ad2ws.wsCheck();}, 15000);
