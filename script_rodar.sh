#!/bin/bash

cd ~/Documentos/TCC/codigos/omnetpp-5.6.2/samples/rtof1

velocidade=('1ms' '15ms' '30ms')
qtde_nos=('3h' '6h' '9h')
tipo_movimento=('linear' 'Circle' 'Random')

for((i_velocidade=0; i_velocidade<${#velocidade[@]}; i_velocidade++)); do
    for((i_qtde=0; i_qtde<${#qtde_nos[@]}; i_qtde++));do
    	for((i_movimento=0; i_movimento<${#tipo_movimento[@]}; i_movimento++));do
    
    		comando='./rtof1 -u Cmdenv -c Wireless_'${tipo_movimento[i_movimento]}'_'${qtde_nos[i_qtde]}'_'${velocidade[i_velocidade]}
    		
    		echo $comando
    		$comando
    
    	done;
    done;
done;
