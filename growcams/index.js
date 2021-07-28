var spawn = require('child_process').spawn;
const express = require('express')
var cors = require('cors')

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

function waitFileExists(filePath, timeout) {
        if (!timeout) timeout = 5000;
        return new Promise(function (resolve, reject) {
            let startAt = Date.now();
            var timer = null;
            const timerFunc = () => {
                    if (!fs.existsSync(filePath)) {
                            if (Date.now() - startAt > timeout) {
                                    reject();
                            } else {
                                    timer = setTimeout(timerFunc, 500);
                            }
                    } else {
                            resolve();
                    }
            };
            timerFunc();
    });
}



class CamServe {
        app;
        v4l2DeviceName;
        ffmpeg = null;
        fps = null;
        camIndex = null;
        timeoutHandle = null;
        tempDir = null;
        httpM3U8Path = null;
        httpStreamPath = null;
        constructor(expressApp, v4l2DeviceName, camIndex, resolution, framerate) {
                this.app = expressApp;
                this.v4l2DeviceName = v4l2DeviceName;
                this.res = resolution || "640x480";
                this.fps = framerate || 25;
                this.camIndex = camIndex;
                this.tempDir = `/tmp/cam${this.camIndex}`;
                this.httpM3U8Path = `/cams/${this.camIndex}/stream.m3u8`;
                this.httpStreamPath = `/cams/${this.camIndex}/stream*.ts`;
        }

        resetTimeout() {
                if (this.timeoutHandle) {
                        clearTimeout(this.timeoutHandle);
                }
                this.timeoutHandle = setTimeout(() => {this.timeoutShutdown();}, FFMPEG_SHUTDOWN_TIMEOUT);
        }

        timeoutShutdown() {
                if (!this.ffmpeg) {
                        return;
                }
                console.log("no requests after a while, killing ffmpeg");
                this.timeoutHandle = null;
                this.killFfmpeg();
        }

        start() {

                this.app.get(this.httpM3U8Path, async (req, res) => {

                        try {
                                if (this.ffmpeg == null) {
                                        await this.startFfmpeg();
                                }
                                this.resetTimeout();

                                await waitFileExists(`${this.tempDir}/stream.m3u8`);

                                await waitFileExists(`${this.tempDir}/stream1.ts`);
                                res.setHeader('Content-Type', 'application/vnd.apple.mpegurl');
                                res.sendFile(`${this.tempDir}/stream.m3u8`);

                        } catch (e) {
                                console.error("Error getting ffmpeg going");
                                console.error(e);
                                res.sendStatus(500);
                                return;
                        }
                });

                this.app.get(this.httpStreamPath, async (req, res) => {
                        if (!this.ffmpeg) {
                                res.sendStatus(404);
                                return;
                        }
                        this.resetTimeout();

                        let file = `${this.tempDir}/${path.basename(req.path)}`
                        try {
                                await waitFileExists(file);
                                res.setHeader('Content-Type', 'video/MP2T');
                                res.sendFile(file);
                        } catch (e) {
                                console.error("Error getting stream");
                                console.error(e);
                                res.sendStatus(500);
                                return;
                        }
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
                if (fs.existsSync(this.tempDir)) {
                        fs.rmSync(this.tempDir, {recursive: true});
                }
                console.log(`creating dir ${this.tempDir}`);
                fs.mkdirSync(this.tempDir, {recursive: true});

                var ffmpegargstr = `-f video4linux2 -video_size ${this.res} -framerate ${this.fps} ` +
                                `-i ${devicePath} ` +
                                `-c:v h264_omx ` +
                                `-f hls ` +
                                `-hls_wrap 4 ${this.tempDir}/stream.m3u8`;
                var ffmpegargs = ffmpegargstr.split(' ');

                console.log("launching ffmpeg with args:");
                console.log(ffmpegargstr);
                this.ffmpeg = spawn('ffmpeg', ffmpegargs, {stdio: ['ignore', 'pipe', 'pipe']});
//console.log("should have launched");
//              this.ffmpeg.stdout.setEncoding("utf8");
//              this.ffmpeg.stdout.on('data', d=> {
//                      console.log(d.toString());
//              });
//              this.ffmpeg.stderr.setEncoding("utf8");
this.ffmpeg.stderr.on('data', function(data) {
    console.log(data.toString());
});
//              this.ffmpeg.stderr.on('data', d=> {
//                      console.error(d.toString());
//              });
//                this.ffmpeg.stdio.on('exit', () => {
//                        console.log('ffmpeg exited');
//                        this.killFfmpeg();
//                });
} catch (e) {
console.log("errors starting ffmpeg");
console.error(e);
}
        }

}


var app = express();
app.use(cors());
var cam1 = new CamServe(app, "GENERAL WEBCAM: GENERAL WEBCAM", 1, "1280x720", 30);
var cam2 = new CamServe(app, "USB2.0 UVC PC Camera: USB2.0 UV", 2, "640x480", 25);
cam1.start();
cam2.start();
app.listen(8081);


