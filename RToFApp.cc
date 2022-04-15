/*
 * RToFApp.cc
 *
 *  Created on: 17 de dez de 2020
 *      Author: allan
 */

#define BUILDING_DLL
#define OPP_DLLIMPORT

#include <chrono>

#include <Eigen/Dense>
#include <iostream>
#include <iomanip>
#include <algorithm>


#include <vector>

#include "inet/applications/base/ApplicationPacket_m.h"
#include "RToFApp.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TagBase_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/lifecycle/ModuleOperations.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"
#include "inet/common/packet/printer/PacketPrinter.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/physicallayer/contract/packetlevel/SignalTag_m.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"
#include "inet/physicallayer/common/packetlevel/Radio.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/physicallayer/base/packetlevel/TransmissionBase_m.h"
#include "inet/common/scheduler/RealTimeScheduler.h"
#include "Backoff_m.h"
#include <fstream>
#include <string>


using namespace inet;

Define_Module(RToFApp);

RToFApp::~RToFApp()
{
    cancelAndDelete(selfMsg);
}

simtime_t backoffTest;

void RToFApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    std::vector<double> xVector;
    std::vector<double> yVector;
    std::vector<double> di;

    if (stage == INITSTAGE_LOCAL) {
        numSent = 0;
        numReceived = 0;
        WATCH(numSent);
        WATCH(numReceived);
        mL_x = 0;
        mL_y = 0;

        overhead=0.001192000001;
        overhead_segundo_envio=0.001192000001+0.000228;
        need_calibration = par("need_calibration");

        arqName = par("arqName");
        localPort = par("localPort");
        destPort = par("destPort");
        isReceiver = par("isReceiver");
        packetName = par("packetName");
        selfMsg = new cMessage("sendTimer");
        num_receptores=par("num_receptores");
    }
}

void RToFApp::setSocketOptions()
{
      socket.setCallback(this);
}

void RToFApp::processStart()
{
    socket.setOutputGate(gate("socketOut"));

    const char *localAddress = par("localAddress");
    socket.bind(localPort);

    if(!isReceiver){
        socket.setBroadcast(false);
    }
    else{
        socket.setBroadcast(true);
    }

    setSocketOptions();

    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);
    const char *token;

    while ((token = tokenizer.nextToken()) != nullptr) {
        if (strstr(token, "Broadcast") != nullptr)
            destAddresses.push_back(Ipv4Address::ALLONES_ADDRESS);
        else {
            L3Address result;
            L3AddressResolver().tryResolve(token, result);
            if (result.isUnspecified())
                EV_ERROR << "cannot resolve destination address: " << token << endl;
            destAddresses.push_back(result);
        }
    }
    if (destAddresses.empty()) {
        EV_ERROR << "Error sending - destAddresses empty" << endl;
    }
    if (isReceiver == false){
        sendPacket();
    }
}

L3Address RToFApp::chooseDestAddr()
{
    int k = intrand(destAddresses.size());
    if (destAddresses[k].isUnspecified() || destAddresses[k].isLinkLocal()) {
        L3AddressResolver().tryResolve(destAddressStr[k].c_str(), destAddresses[k]);
    }
    return destAddresses[k];
}

void RToFApp::sendPacket()
{
    std::ostringstream str;

    str << packetName << "-" << numSent;

    Packet *packet = new Packet(str.str().c_str());

    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(4));
    payload->setSequenceNumber(numSent);
    payload->addTag<CreationTimeTag>()->setCreationTime(simTime());

    packet->insertAtBack(payload);
    L3Address destAddr = chooseDestAddr();

    cModule *host = getContainingNode(this);
    std::cout << "host: " << host << endl;

    emit(packetSentSignal, packet);
    socket.sendTo(packet, destAddr, destPort);

    numSent++;

}

void RToFApp::processStop()
{
    socket.close();
}

void RToFApp::handleMessageWhenUp(cMessage *msg)
{
        socket.processMessage(msg);
}

void RToFApp::finish()
{
    ApplicationBase::finish();
}

void RToFApp::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    processPacket(packet);
}

void RToFApp::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring UDP error report " << indication->getName() << endl;
    delete indication;
}

void RToFApp::socketClosed(UdpSocket *socket)
{
    if (operationalState == State::STOPPING_OPERATION)
        startActiveOperationExtraTimeOrFinish(par("stopOperationExtraTime"));
}

void RToFApp::refreshDisplay() const
{
    ApplicationBase::refreshDisplay();
    char buf[100];
    sprintf(buf, "rcvd: %d pks\nsent: %d pks", numReceived, numSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void RToFApp::processPacket(Packet *pk)
{
    emit(packetReceivedSignal, pk);
    EV_INFO << "Received packet: " << UdpSocket::getReceivedPacketInfo(pk) << endl;

    if(isReceiver){
        processPacketAnchors(pk);
    }else{
        processPacketIssuer(pk);
    }
    numReceived++;
    delete pk;

}

void RToFApp::handleStartOperation(LifecycleOperation *operation)
{
    processStart();
}


void RToFApp::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(selfMsg);
    socket.close();
    delayActiveOperationFinish(par("stopOperationTimeout"));
}

void RToFApp::handleCrashOperation(LifecycleOperation *operation)
{
    cancelEvent(selfMsg);
    socket.destroy();         //TODO  in real operating systems, program crash detected by OS and OS closes sockets of crashed programs.
}

double RToFApp::distanceCalc(simtime_t finalT, simtime_t iniT, simtime_t overhead, simtime_t backoff)
{
    double distance = (((299792458 * (finalT - iniT - overhead - backoff ).dbl())/2.0)); // overhead, this was found through an environment with two hosts at a distance of 1m, thus calculating the distance, the result with overhead was subtracted of the real distance

    timeFlight.push_back((overhead + backoff ).dbl());
    return distance;
}

void RToFApp::mL(){
    int numH = xVector.size();
    Eigen::MatrixXd A(numH - 1, 2);
    Eigen::MatrixXd b(numH - 1, 1);
    for(int i = 0; i < numH - 1; i++){
        A(i,0)=2*(xVector[numH - 1] - xVector[i]);
    }
    for(int i = 0; i < numH - 1; i++){
        A(i,1)=2*(yVector[numH - 1] - yVector[i]);
    }
    // std::cout << "testing matrix: "<< endl << A << endl;

    for(int i = 0; i < numH - 1; i++){
        b(i,0) = (-(pow(xVector[i],2)) - (pow(yVector[i],2)) + (pow(di[i],2))) - (-(pow(xVector[numH - 1],2)) - (pow(yVector[numH - 1],2)) + pow(di[numH - 1],2));
    }


    Eigen::MatrixXd A_i(2,2);
    Eigen::MatrixXd A_t(2,2);
    A_t = A.transpose();
//    std::cout << "Resultado A_t: " << endl << A_t <<endl;
    A_i = (A_t * A).inverse();
    A = A_i * A_t * b;
    mL_x = A(0,0);
    mL_y = A(1,0);
    std::cout << "Resultado ML x,y: " << endl << mL_x << ", " << mL_y <<endl;
}


void RToFApp::saveXPoints(const char *local){
    int i;
    char x[9];
    char aux[] = ",";
    int j = 0;
    std::cout << "----TESTE local: " << local << endl;
    for(i = 0;local[i+1] != aux[0];i++){
        x[i] = local[i+1];
    }

    x[i] = '\0';
//    std::cout << "----TESTE X: " << x << endl;
    xVector.push_back(atof(x));
//    for (i = 0; i < xVector.size(); i++)
//    {
//        std::cout << "VECTORRRR X: " << xVector[i] << "," << endl;
//    }
//    std::cout << " " << endl;

}

void RToFApp::saveYPoints(const char *local){
    char y[9];
    char aux[] = ",";
    int j = 0;
    int k = 0;

    for(int i = 0;local[i] != aux[0];i++){
       j = i;
    }

    for(int i = j+2; i< strlen(local); i++){
        y[k] = local[i];
        k++;
    }
    y[k] = '\0';

    yVector.push_back(atof(y));
}

const char* RToFApp::ConvertDoubleToString(double value1, double value2){
    std::stringstream ss ;
    ss << value1 << "," << value2;
    const char* str = ss.str().c_str();
    return str;
}

omnetpp::simtime_t RToFApp::Calibration(simtime_t StartT, simtime_t EndT, simtime_t backoffTime){
    auto overhead = (EndT - StartT - backoffTime) - ((2*sqrt(1)) / 299792458.0); //((2 * 1) / 299792458) here we have the calc of real time, so we subtract this real time from time with overhead and we find the overhead
    return overhead;
}

void RToFApp::setIniTime(simtime_t time)
{
    IniTime = time;
}

void RToFApp::processPacketAnchors(Packet *pk){
    //getting address
    auto l3Addresses = pk->getTag<L3AddressInd>();
    L3Address destAddr = l3Addresses->getSrcAddress();


    auto signalTimeTag = pk->getTag<SignalTimeInd>();

    std::ostringstream str;
    str << packetName << "-" << numSent;

    Packet *packet = new Packet(str.str().c_str());
    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(4));
    payload->setSequenceNumber(numSent);

    //getting the real position of nodes
    cModule *host = getContainingNode(this);
    IMobility *mobility = check_and_cast<IMobility *>(host->getSubmodule("mobility"));
    auto real_position = mobility->getCurrentPosition();

    packet->setName(real_position.str().c_str());

    packet->setTimestamp(signalTimeTag->getStartTime());

    auto tags=payload->addTag<CreationTimeTag>();
    tags->setCreationTime(signalTimeTag->getEndTime());

    packet->insertAtBack(payload);

    emit(packetSentSignal, packet);
    socket.sendTo(packet, destAddr, destPort);

    numSent++;
}

void RToFApp::processPacketIssuer(Packet *pk){
    broadcastTime = IniTime;

    saveXPoints(pk->getFullName());
    saveYPoints(pk->getFullName());

    auto backoffTime = pk->getTag<backoff>(); //getting backoffTime do CSMA


    if(need_calibration == 1){
        auto overhead = Calibration(broadcastTime, pk->getArrivalTime(), backoffTime->getBackoffTime() );//overhead from environment with two hosts is 0.001182000001
        std::cout << "overhead: " << overhead << endl;
    }


    // backoffTest += backoffTime->getBackoffTime();

    //std::cout << "TESTE do CSMA: " << backoffTime->getBackoffTime() << " TESTE do cancel Time: " << backoffTime->getInitialBackoffTime() << endl;

    //auto measuredWithNoise = pk->getArrivalTime() + host->intuniform(-2,2)*0.0000000125;
    auto measuredWithNoise = pk->getArrivalTime();

    // std::cout << " measuredWithNoise"<<measuredWithNoise << endl;

    //0.001192000001
    auto dist = distanceCalc(measuredWithNoise, broadcastTime, overhead, backoffTime->getBackoffTime());
    std::cout << "Distance between hosts getting auto backoff = " << dist << endl;

    std::cout << " " << endl;
    std::cout << " " << endl;

    di.push_back(dist);

    for (unsigned int i = 0; i < yVector.size(); i++)
    {
        std::cout << "di: " << di[i] <<",   Time Flight: " << timeFlight[i] << endl;
    }

    cModule *host = getContainingNode(this);
    IMobility *mobility = check_and_cast<IMobility *>(host->getSubmodule("mobility"));
    auto real_position = mobility->getCurrentPosition();
    realPosition.x=real_position.x;
    realPosition.y=real_position.y;

    //Verifica se recebeu as mensagens de todos os nós que deveria
    if(numReceived==(num_receptores-1)){
        mL();
        insertToCsv();

        yVector.clear();
        xVector.clear();
        di.clear();

        numReceived=-1;
        sendPacket();

        overhead=overhead_segundo_envio;

        setIniTime(simTime());
    }
}


void RToFApp::insertToCsv(){
    if (!isReceiver){
        std::ifstream arq;
        std::ofstream myfile;

        arq.open("planilha_dados/"+std::string(arqName));

        auto erro=calculationError();

        if(arq.is_open()){
            myfile.open ("planilha_dados/"+std::string(arqName),std::ios::app);

            myfile<<mL_x<<","<<mL_y<<",";
            myfile<<realPosition.x<<","<<realPosition.y<<",";
            myfile<<erro<<",";

            myfile<<"\n";
        }else{
            myfile.open("planilha_dados/"+std::string(arqName));


            myfile << "Likelihood position X (m),";
            myfile << "Likelihood position Y (m),";
            myfile << "Real position X (m),";
            myfile << "Real position Y (m),";
            myfile << "Erro";

            myfile<<"\n";

            myfile<<mL_x<<","<<mL_y<<",";
            myfile<<realPosition.x<<","<<realPosition.y<<",";
            myfile<<erro<<",";

            myfile<<"\n";
        }
        myfile.close();
        arq.close();
    }
}

double RToFApp::calculationError(){

    auto subtracao_x=realPosition.x - mL_x;
    auto subtracao_y=realPosition.y - mL_y;

    auto x_elevado= pow(subtracao_x, 2);
    auto y_elevado= pow(subtracao_y, 2);

    auto erro= sqrt(x_elevado+y_elevado);

    return erro;
}
