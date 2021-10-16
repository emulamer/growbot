var spawn = require('child_process').spawn;
const express = require('express')
var cors = require('cors')
const m2j = require('mjpeg2jpegs');
const path = require('path')
const fs = require('fs');
const net = require('net');
const ws = require('ws');

const FFMPEG_SHUTDOWN_TIMEOUT = 10000



function getDeviceMap() {
        const v4lSysPath = '/sys/class/video4linux/'
        const dirs = fs.readdirSync(v4lSysPath);
        const deviceMap = {};
        for (let vdir of dirs) {
                if (!vdir.startsWith('video')) {
                        continue;
                }
                const path = `${v4lSysPath}/${vdir}/name`;
                try {
                        const name = fs.readFileSync(path).toString().trim();
                        if (!deviceMap[name]) {
                                deviceMap[name] = vdir;
                        }
                }
                catch (e) {
                        console.error(`error reading name of ${path}`);
                        console.error(e);
                }
        }
        console.log("device map:");
        console.log(deviceMap);
        return deviceMap;
}

function getDevForName(name) {
        const map = getDeviceMap();
        if (!map[name]) {
                throw `Device name ${name} not found!`;
        }
        return map[name];
}


class CamServe {
        latestJpeg = null;
        app;
        v4l2DeviceName;
        ffmpeg = null;
        fps = null;
        camIndex = null;
        timeoutHandle = null;
        httpStreamPath = null;
        constructor(expressApp, v4l2DeviceName, camIndex, resolution, framerate) {
                this.app = expressApp;
                this.v4l2DeviceName = v4l2DeviceName;
                this.res = resolution || "640x480";
                this.fps = framerate || 25;
                this.camIndex = camIndex;
                this.httpStreamPath = `/cams/${this.camIndex}/jpg.jpg`;
        }

        resetTimeout() {
                if (this.timeoutHandle) {
                        clearTimeout(this.timeoutHandle);
                }
                this.timeoutHandle = setTimeout(() => {this.timeoutShutdown();}, FFMPEG_SHUTDOWN_TIMEOUT);
        }

        timeoutShutdown() {
                this.latestJpeg = null;
                if (!this.ffmpeg) {
                        return;
                }
                console.log("no requests after a while, killing ffmpeg");
                this.timeoutHandle = null;
                this.killFfmpeg();
        }

        start() {
                this.app.get(this.httpStreamPath, async (req, res) => {
                        if (!this.ffmpeg) {
                                this.startFfmpeg();
                        }
                        this.resetTimeout();
                        let handle;
                        let handle2;
                        let doWaitForFile;
                        doWaitForFile = () => {
                                if (this.latestJpeg != null) {
                                        clearTimeout(handle2);
                                        res.status(200);
                                        res.setHeader("Content-Type", "image/jpeg");
                                        res.setHeader("Content-Length", this.latestJpeg.length);

                                        res.write(this.latestJpeg);
                                        res.end();
                                        handle = null

                                } else {
                                        handle = setTimeout(() => {
                                                doWaitForFile();
                                        }, 100);
                                }
                        };
                        handle2 = setTimeout(() => {if (handle != null) {
                                clearTimeout(handle);
                                handle = null;
                                res.sendStatus(500);
                                return;
                        }}, 1000);
                        doWaitForFile();
                });
        }


        killFfmpeg() {
                try {
                        if (this.ffmpeg == null) {
                                console.log("ffmpeg already stopped");
                        } else {
                                console.log("stopping ffmpeg");
                                this.ffmpeg.kill();
                        }
                } finally {
                        this.ffmpeg = null;
                }
                try {
                        if (fs.existsSync(this.tempDir)) {
                                fs.rmSync(this.tempDir, {recursive: true});
                        }
                } catch (e) {
                        console.error("Temp directory cleanup failed");
                        console.error(e);
                }
        }

        async startFfmpeg() {
                try {
                        let devicePath = "/dev/" + getDevForName(this.v4l2DeviceName);

                        if (this.ffmpeg != null) {
                                console.warn("tried to start ffmpeg but it is running!");
                                return;
                        }
                        var ffmpegargstr = `
                        -f video4linux2
                        -video_size ${this.res}
                        -framerate ${this.fps}
                        -i ${devicePath}
                        -q:v 9
                        -s ${this.res}
                        -r ${this.fps}
                        -f mjpeg
                        pipe:1`
                        var ffmpegargs = ffmpegargstr.replace(/\n/g, ' ').split(' ').filter(x=> x.trim().length > 0).map(x=> x.trim());
                        console.log("launching ffmpeg with args:");
                        console.log(ffmpegargs);
                        this.ffmpeg = spawn('ffmpeg', ffmpegargs);


                        this.ffmpeg.stderr.on('data', function(data) {
                                console.log(data.toString());
                        });
                        let tmpBuffer = null;
                        this.ffmpeg.stdout.on("data", d=> {
                                //boy this sure is a half assed implementation of this
                                let hasStart = (d.readUInt8(0) == 0xFF && d.readUInt8(1) == 0xd8);
                                let hasEnd = (d.readUInt8(d.length - 2) == 0xFF && d.readUInt8(d.length - 1) == 0xd9);
                                if (hasStart && !hasEnd) {
                                        tmpBuffer = d;
                                }
                                else if (hasEnd && !hasStart && tmpBuffer) {
                                        this.latestJpeg = Buffer.concat([tmpBuffer, d]);
                                        tmpBuffer = null;
                                } else if (!hasEnd && !hasStart && tmpBuffer) {
                                        tmpBuffer = Buffer.concat([tmpBuffer, d]);
                                } else if (hasStart && hasEnd) {
                                        this.latestJpeg = d;
                                }
                        });
                } catch (e) {
                        console.log("errors starting ffmpeg");
                        console.error(e);
                }
        }

}


var app = express();
app.use(cors());
var cam1 = new CamServe(app, "GENERAL WEBCAM: GENERAL WEBCAM", 1, "1280x720", 10);
var cam2 = new CamServe(app, "USB2.0 UVC PC Camera: USB2.0 UV", 2, "640x480", 10);
cam1.start();
cam2.start();
app.listen(8081);
