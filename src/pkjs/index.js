var DarkskyKey = localStorage.getItem("DarkskyKey");

var Clay = require('pebble-clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

Pebble.addEventListener('showConfiguration', function(e) {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && !e.response) {return;}
  var json_resp = JSON.parse(e.response);
  DarkskyKey = json_resp.DarkskyKey.value;
  localStorage.setItem("DarkskyKey", DarkskyKey);
});

var iconNameToId = {
  'clear-day': 1,
  'clear-night': 2,
  'rain': 3,
  'snow': 4,
  'sleet': 5,
  'wind': 6,
  'fog': 7,
  'cloudy': 8,
  'partly-cloudy-day': 9,
  'partly-cloudy-night': 10
};

function sendWeather() {
    if (DarkskyKey!==null) {
        navigator.geolocation.getCurrentPosition(function (pos){
            var req = new XMLHttpRequest();
            req.addEventListener("load", function (){
              // TODO Fix the message key issue and use descriptive keys!
              var json = {0:iconNameToId[req.response.currently.icon],                         // id - check the c source
                          1:Math.round(req.response.currently.apparentTemperature),            // Celsius
                          2:Math.round(req.response.daily.data[0].apparentTemperatureMax),     // Celsius
                          3:Math.round(req.response.daily.data[0].apparentTemperatureMin),     // Celsius
                          4:Math.round(req.response.daily.data[0].precipProbability*100),      // Percents
                          // cm/h scaled to a byte, >7.6 mm/h is the definition of heavy rain
                          5:req.response.minutely.data.map(function (el){return Math.min(Math.round(el.precipIntensity/10*255), 255);}).slice(0,60)
                         };
/*
              var json = {WEATHER_ICON_KEY:iconNameToId[req.response.currently.icon],                         // id - check the c source
                          WEATHER_TEMPERATURE_KEY:Math.round(req.response.currently.apparentTemperature),            // Celsius
                          WEATHER_TEMPERATUREMAX_KEY:Math.round(req.response.daily.data[0].apparentTemperatureMax),     // Celsius
                          WEATHER_TEMPERATUREMIN_KEY:Math.round(req.response.daily.data[0].apparentTemperatureMin),     // Celsius
                          WEATHER_PRECIP_PROB_KEY:Math.round(req.response.daily.data[0].precipProbability*100),      // Percents
                          // cm/h scaled to a byte, >7.6 mm/h is the definition of heavy rain
                          WEATHER_PRECIP_ARRAY_KEY:req.response.minutely.data.map(function (el){return Math.min(Math.round(el.precipIntensity/10*255), 255);}).slice(0,60)
                         };
*/
              Pebble.sendAppMessage(json);
            });
            req.responseType = 'json';
            req.open("GET", "https://api.darksky.net/forecast/"+DarkskyKey+"/"+pos.coords.latitude+","+pos.coords.longitude+"?units=si");
            req.send();
        });
    }
}

Pebble.addEventListener("ready", function() {sendWeather(); setInterval(sendWeather, 10*60*1000);});