//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.INetworkNode;
import inet.physicallayer.contract.packetlevel.IRadioMedium;
import inet.visualizer.contract.IIntegratedVisualizer;

network Wireless6h
{
    parameters:
        @display("bgb=709.632,430.41602;bgg=100,1,grey95");
        @figure[title](type=label; pos=0,-1; anchor=sw; color=darkblue);

        @figure[rcvdPkText](type=indicatorText; pos=380,20; anchor=w; font=,18; textFormat="packets received: %g"; initialValue=0);

    submodules:
        visualizer: <default("IntegratedCanvasVisualizer")> like IIntegratedVisualizer if hasVisualizer() {
            @display("p=580,125");
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=580,200");
        }
        radioMedium: <default("UnitDiskRadioMedium")> like IRadioMedium {
            @display("p=580,275");
        }
        host1: <default("WirelessHost")> like INetworkNode {
            @display("p=100,100");
        }
        host2: <default("WirelessHost")> like INetworkNode {
            @display("p=110,100");
        }
        host3: <default("WirelessHost")> like INetworkNode {
            @display("p=90,100");
        }
        host4: <default("WirelessHost")> like INetworkNode {
            @display("p=100,110");
        }
        host5: <default("WirelessHost")> like INetworkNode {
            @display("p=100,90");
        }
        host6: <default("WirelessHost")> like INetworkNode {
            @display("p=90,90");
        }
        host7: <default("WirelessHost")> like INetworkNode {
            @display("p=110,110");
        }
}
