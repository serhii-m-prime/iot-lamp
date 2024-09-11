window.addEventListener('load', function() {
    Index.init();
});

let Index = {
    init: function() {
        this.buildDevicesTabs();
    },
    buildDevicesTabs: async function() {
        let response = await fetch('/devices');
        let devices = await response.json();
        document.getElementById('pills-tab').innerHTML = '';
        document.getElementById('pills-tabContent').innerHTML = '';
        for (let device of devices['devices']) {
            if (device.isMaster) {
                document.getElementById('main-node').setAttribute('href', `http://${device.ip}/`);
            }
            let tab = document.createElement('li');
            let deviceId = device.mac.replace(/:/g, '');
            tab.classList.add('nav-item');
            tab.setAttribute('role', 'presentation');
            tab.innerHTML = `<a class="nav-link" id="pills-${deviceId}-tab" data-toggle="pill" href="#pills-${deviceId}" role="tab" aria-controls="pills-${deviceId}" aria-selected="false">${device.name}</a>`;
            document.getElementById('pills-tab').appendChild(tab);
            let tabContent = `<div class="tab-pane fade show active" id="pills-${deviceId}" role="tabpanel" aria-labelledby="pills-${deviceId}-tab" tabindex="0"><iframe src="http://${device.ip}/stats" frameborder="0" style="height: 60vh; width: 100%;" ></iframe></div>`
            document.getElementById('pills-tabContent').innerHTML += tabContent;
        }
    }
};