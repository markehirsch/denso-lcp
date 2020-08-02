#include "DensoController.cpp"
#include <math.h>
#include <time.h>
#include <chrono>
#include <fstream>

int main() {
    std::string filename = "../data/bottle.36.traj";

    //////////////////// Initialize DENSO controller ////////////////////
    DensoController::DensoController denso;
    denso.bCapEnterProcess();
    BCAP_HRESULT hr;

    //////////////////// Get trajectorystring from a file ////////////////////
    std::ifstream myfile(filename);
    std::string temp;
    std::string trajectorystring;
    std::getline(myfile, temp);
    trajectorystring += temp;
    while (std::getline(myfile, temp)) {
        trajectorystring += "\n";
        trajectorystring += temp;
    }
    TOPP::Trajectory *ptraj = new TOPP::Trajectory(trajectorystring);
    std::vector<double> q(ptraj->dimension);
    std::vector<double> tmp;

    //// VERY IMPORTANT :: MOVE TO INITIAL POSE BEFORE EXECUTING TRAJ IN SLAVE MODE ////
    hr = denso.SetExtSpeed("100");
    std::cout << "Moving to the initial pose...\n";

    ptraj->Eval(0.0, q);
    tmp = DensoController::VRad2Deg(q);
    std::string commandstring;
    const char* command;
    commandstring = "J(" + std::to_string(tmp[0]) + ", " + std::to_string(tmp[1])
                    + ", " + std::to_string(tmp[2]) + ", " + std::to_string(tmp[3])
                    + ", " + std::to_string(tmp[4]) + ", " + std::to_string(tmp[5]) + ")";
    command = commandstring.c_str(); // convert string -> const shar*
    std::cout << commandstring << "\n";
    denso.bCapRobotMove(command, "Speed = 25");
    sleep(5);

    hr = denso.bCapRobotExecute("ClearLog", ""); // enable control logging

    //////////////////// Build a LUT for the Trajectory ////////////////////
    double s = 0.0;
    double slower = 1.0;
    double timestep = 8.0*(1e-3)*slower;

    std::string tablestring = "";
    std::string separator1 = "";
    std::string separator2 = "";

    std::stringstream t;
    std::stringstream timestamp;
    std::vector<std::vector<double> > LUT;
    std::cout << "Building a look-up table for the trajectory. . ." << "\n";
    while (s < 1.0) {
        ptraj->Eval(s, q);
        LUT.push_back(q);
        tablestring += separator1;
        tablestring += std::to_string(q[0]) + " " + std::to_string(q[1])
                       + " " + std::to_string(q[2]) + " " + std::to_string(q[3])
                       + " " + std::to_string(q[4]) + " " + std::to_string(q[5]);
        separator1 = "\n";

        t << std::setprecision(17) << separator2 << s;
        separator2 = " ";
        s += 0.8*timestep;
    }

    // while (s < 1.9) {
    //     ptraj->Eval(s, q);
    //     LUT.push_back(q);
    //     tablestring += separator;
    //     tablestring += std::to_string(q[0]) + ", " + std::to_string(q[1])
    //                    + ", " + std::to_string(q[2]) + ", " + std::to_string(q[3])
    //                    + ", " + std::to_string(q[4]) + ", " + std::to_string(q[5]);
    //     separator = "\n";

    //     t << std::setprecision(17) << s << " ";
    //     s += 1.0*timestep;
    // }

    while (s < ptraj->duration) {
        ptraj->Eval(s, q);
        LUT.push_back(q);
        tablestring += separator1;
        tablestring += std::to_string(q[0]) + " " + std::to_string(q[1])
                       + " " + std::to_string(q[2]) + " " + std::to_string(q[3])
                       + " " + std::to_string(q[4]) + " " + std::to_string(q[5]);
        separator1 = "\n";

        t << std::setprecision(17) << separator2 << s;
        s += 1.0*timestep;
    }

    std::ofstream out("../data/bottle36-mod.table");
    out << tablestring;
    out.close();

    int nsteps = LUT.size();

    ////////////////////////////// BEGIN SLAVE MODE //////////////////////////////
    hr = denso.bCapSlvChangeMode("514");

    BCAP_VARIANT vntPose, vntReturn;
    std::vector<BCAP_VARIANT> history;
    history.resize(0);

    std::stringstream trealtime;
    struct timespec tic, toc;

    s = 0.0;
    q = LUT[0];
    vntPose = denso.VNTFromRadVector(q);
    clock_gettime(CLOCK_MONOTONIC, &tic); //TIC
    hr = bCap_RobotExecute2(denso.iSockFD, denso.lhRobot, "slvMove", &vntPose, &vntReturn);
    history.push_back(vntReturn);

    for (int i = 0; i < nsteps; i++) {
        q = LUT[i];
        vntPose = denso.VNTFromRadVector(q);
        clock_gettime(CLOCK_MONOTONIC, &toc); //TOC
        s += (toc.tv_sec - tic.tv_sec) + (toc.tv_nsec - tic.tv_nsec)/nSEC_PER_SECOND;
        trealtime << std::setprecision(17) << s << " ";
        clock_gettime(CLOCK_MONOTONIC, &tic); //TIC
        hr = bCap_RobotExecute2(denso.iSockFD, denso.lhRobot, "slvMove", &vntPose, &vntReturn);
        history.push_back(vntReturn);
    }

    clock_gettime(CLOCK_MONOTONIC, &toc); //TOC
    s += (toc.tv_sec - tic.tv_sec) + (toc.tv_nsec - tic.tv_nsec)/nSEC_PER_SECOND;
    trealtime << std::setprecision(17) << s << " ";


    ////////////////////////////// STOP SLAVE MODE //////////////////////////////
    hr = denso.bCapSlvChangeMode("0");

    ////////////////////////////// SAVE ENCODER DATA //////////////////////////////
    std::stringstream ss;
    ss << std::setprecision(17) << history[0].Value.DoubleArray[0];
    for (int k = 1; k < 6; k++) {
        ss << " " << std::setprecision(17) << history[0].Value.DoubleArray[k];
    }
    ss << "\n";
    for (int i = 1; i < int(history.size()); i++) {
        ss << std::setprecision(17) << history[i].Value.DoubleArray[0];
        for (int j = 1; j < 6; j++) {
            ss << " " << std::setprecision(17) << history[i].Value.DoubleArray[j];
        }
        ss << "\n";
    }
    std::ofstream out1("densohistory.traj");
    out1 << ss.str();
    out1.close();
    std::cout << "waypoints successfully written in denhistory.traj\n";

    std::ofstream out2("densohistory.timestamp");
    out2 << t.str();
    out2.close();
    std::cout << "timestamp successfully written in denhistory.timestamp\n";

    std::ofstream out3("densohistory.realtimestamp");
    out3 << trealtime.str();
    out3.close();
    std::cout << "realtimestamp successfully written in denhistory.timestamp\n";


    ////////////////////////////// EXIT B-CAP PROCESS //////////////////////////////
    hr = denso.bCapRobotExecute("StopLog", "");
    denso.bCapExitProcess();
}
