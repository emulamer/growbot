const sqlite3 = require("sqlite3").verbose();
//import * as sqlite3 from "sqlite3";

const db = new sqlite3.Database("./nodered-sqlite");

let sql = "";
let rowObjs = [];
let cols = [];
var prep = db.prepare("select state_json from states order by timestamp asc");
try {
prep.each((err, row) => {
    if (err) {
        console.error(err);
        return true;
    }
    let rowobj = {}
    const addOne = (field, val) => {
        if (!cols.includes(field)) {
            cols.push(field);
        }
        rowobj[field] = val;
    };
    let fields = "";
    let values = "";
    let obj = JSON.parse(row.state_json);
    for (let key of Object.keys(obj)) {
        let val = obj[key];
        let fname = key;
        if (val == null) {
           // fields += fname + ",";
            //values += "null,";
            addOne(fname, "null");
        }
        else if (typeof val === "object") {
            for (let subkey of Object.keys(val)) {
                if (subkey == "buckets" || subkey == "bucketWaterLevelCalibration" || subkey == "pumpPercent" || subkey == "pumpOn" || subkey == "pumpCalibration") {
                    continue;
                }
                let subval = val[subkey];
                let subfname = fname + "_" + subkey;
                if (subval == null) {
                    //fields += subfname + ",";
                    //values += "null,";
                    addOne(subfname, "null");
                }
                else if (typeof subval === "object") {
                    for (let subsubkey of Object.keys(subval)) {
                        let subsubval = subval[subsubkey];
                        let subsubfname = subfname + "_" + subsubkey;
                        //fields += subsubfname + ",";
                        if (subsubval === null) {
                            //values += "null,";
                            addOne(subsubfname, "null");
                        }
                        else if (typeof subsubval === "string") {
                            addOne(subsubfname, "'" + subsubval.replace(/\'/g,"''") + "'");                            
                        } else if (Number.isNaN(subsubval)) {
                            addOne(subsubfname, "NaN");
                        } else {
                            addOne(subsubfname, subsubval.toString());
                        }                        
                    }
                } else {
                        if (subval === null) {
                            addOne(subfname, "null");
                        }
                        else if (typeof subval === "string") {
                            addOne(subfname, "'" + subval.replace(/\'/g,"''") + "'");
                        } else if (Number.isNaN(subval)) {
                            addOne(subfname, "NaN");
                        } else {
                            addOne(subfname, subval.toString());
                        }
                }
            }
        } else {
            if (val === null) {
                addOne(fname, "null");
            }
            else if (typeof val === "string") {
                addOne(fname, "'" + val.replace(/\'/g,"''") + "'");
            } else if (Number.isNaN(val)) {
                addOne(fname, "NaN");
            } else {
                addOne(fname, val.toString());
            }
        }
    }
    rowObjs.push(rowobj);
}, () => {
    console.log("got here");
    let sqlStart = "insert into data (";
    for (let c of cols) {
        sqlStart += c + ",";
    }
    sqlStart = sqlStart.substr(0, sqlStart.length - 1);
    sqlStart += ") VALUES ";
    console.log(sqlStart);
    
    let ctr = 0;
    for (let r of rowObjs) {
        sql += "(";
        for (let c of cols) {
            if (typeof r[c] === 'undefined') {
                sql +="null";
            } else {
                if (r[c] === "true") {
                    sql +="1";
                } else if (r[c] === "false") {
                    sql += "0";
                } else {
                    sql += r[c] ;
                }
            }
            if (c != cols[cols.length-1]) {
                sql += ",";
            }
        }
        sql +=  ")"
        
            sql += ",";
        
        ctr++;
        if (ctr > 0 && ctr % 1000 == 0) {
            sql = sqlStart + sql;
            if (sql.endsWith(","))     {
                sql = sql.substr(0, sql.length-1);
            }
            console.log("doin sql");
            
            db.exec(sql, (err, st) => {
                if (err) {
                console.error(err);
                } else {
                    console.log(st);
                }
            })
            sql = "";
        }
        //console.log(ctr);
    }
    sql = sqlStart + sql;
    if (sql.endsWith(","))     {
        sql = sql.substr(0, sql.length-1);
    }
    console.log("doin sql");
    db.exec(sql, (err, st) => {
        console.log("done");
        if (err) {
            
        console.error(err);
        console.log("sql was " + sql);
        } else {
            console.log("sql was " + sql);
            console.log(st);
        }
    })
});
} catch (e) {
    console.error(e);
}


