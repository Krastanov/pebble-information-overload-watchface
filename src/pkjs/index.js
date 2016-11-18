var key = "5fb81bfcb669ce38966773d530ba8572";

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

// TODO fix abomination
function abominationThatShouldNotBeNecessary(data) {
    var a = [];
    for (var i=0; i<60; i++) {
        a.push(data[i]);
    }
    return a;
}

function sendWeather() {
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
                      5:abominationThatShouldNotBeNecessary(req.response.minutely.data).map(function (el){return Math.min(Math.round(el.precipIntensity/10*255), 255);}).slice(0,60)
                     };
          Pebble.sendAppMessage(json);
        });
        req.responseType = 'json';
        req.open("GET", "https://api.darksky.net/forecast/"+key+"/"+pos.coords.latitude+","+pos.coords.longitude+"?units=si");
        req.send();
    });
}

Pebble.addEventListener("ready", function() {sendWeather(); setInterval(sendWeather, 15*60*1000);});