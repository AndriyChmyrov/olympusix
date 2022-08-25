# olympusix
Matlab MEX wrapper written in C/C++ (MS Visual Studio) for Olympus IX-series motorized microscope frame (tested on Olympus IX83)

Requires Olympus SDK (PortManager software with fsi1394.dll, gt_log.dll, msl_pd_1394.dll, msl_pm.dll and gtlib.config), 
which has to be obtained from an Olympus representative

Olympus SDK commands and operation principles are described in files IX3-BSW Application Note_E.pdf, IX3TPCif_E.pdf, IS3_cmd_v6_E.pdf, which are available from Olympus.

Overall, control of an Olympus IX83 microscope frame through SDK or even this Matlab MEX wrapper is not an easiest thing, but it is doable.
Contact me for additional details, if needed.

Syntaxis:
~~~Matlab
olmypusix('Olympus SDK command')            % send Olympus SDK command to the microscope frame (I3-TPC unit)
olmypusix('setParameterCommand',value); 	% set Parameter to provided value or execute command
result = olmypusix('getParameterCommand');	% get Parameter value or get command output
~~~

| Parameters/Commands:	| Call type |
| :---					| :----:	|
| close					| get only	|
| focusposition			| set&get	|
| focusposition_um		| set&get	|
| frame_lights			| set&get	|
| lightpath				| set&get	|
| maindeck				| set&get	|
| mirrordeck1			| set&get	|
| mirrordeck2			| set&get	|
| objective				| set&get	|
| observationmethod		| set only	|
| observationmethods	| set only	|
| omparameters			| set only	|
| open					| get only	|
| panel_lights			| set&get	|
| reset					| get only	|
| responded				| get only	|
| response				| set&get	|
| responseread			| get only	|
| shutterdeck1			| set&get	|
| shutterdeck2			| set&get	|
| waitforresponse		| get only	|
| windowstatus			| get only	|


**Example:**
~~~Matlab
olympusix('open');                 % open connection to Olympus IX83
olympusix('L 1,0');                % login to I3-TPC touch panel
olympusix('EN6 1,1');              % Olympus SDK command - enable jog wheel on U-MCZ unit
olympusix('EN5 1');                % Olympus SDK command - enable TPC
olympusix('OPE 1');                % Olympus SDK command - enter configuration mode
olympusix('S_U1 2,1,N,N,1,O');     % Olympus SDK command - set settings for DIA - all to None
olympusix('S_U2 N,N,N,N');         % Olympus SDK command - set settings for DIA - all to None
olympusix('S_MU2 N,N,N,N');        % Olympus SDK command - set available mirror names for Deck2
olympusix('S_OMCP Fluo,Scan,Cam'); % Olympus SDK command - set names of the observation modes
olympusix('OPE 0');                % Olympus SDK command - leave configuration mode: after this Z-drive control works
olympusix('NFP 1');                % Olympus SDK command - focus notification on
olympusix('N 1');                  % Olympus SDK command - all other notifications on 
olympusix('SK 1');                 % Olympus SDK command - host key notification on
olympusix('NOB 1');                % Olympus SDK command - objective lens change notification
olympusix('C 1');                  % Olympus SDK command - active notification of the cover switch status of IX3 – RFACA
olympusix('SKD 354,1');            % Olympus SDK command - enable Escape button on TPC unit
olympusix('DIL2 0');               % Olympus SDK command - set transmitted illumination lamp brightness to 0

olympusix('FSPD 70000,300000,60'); % Olympus SDK command - set Z drive speed. parameters = initial_speed, constant_speed, accel_time
olympusix('FG 6000');              % Olympus SDK command - Absolute movement of Z to 60.00[um]
olympusix('FM N,1000');            % Olympus SDK command - Relative movement of Z to +10.00[um]
olympusix('FM F,1000');            % Olympus SDK command - Relative movement of Z to -10.00[um]
olympusix('FP?');                  % Olympus SDK command - get local coordinate

% set names of observation methods 
olympusix('observationmethods','FL,BF'); 
% set parameters of the observation methods for the OMSEQ command, (page 92 of IX3TPCif_E.pdf)
olympusix('omparameters',{'1,0,N,N,N,N,N,1,N,N,8,N,N,N,N,N,N,N','1,0,N,N,N,N,N,3,N,N,1,N,N,N,N,N,N,N'});

olympusix('focusposition')         % get focus Z position in Olympus units. Multiply that value by 1e-8 to convert to meters
olympusix('focusposition',zpos/1e-8); % set focus Z position. The parameter zpos is in meters
olympusix('frame_lights',0);       % switch off lights on the IX83 frame
olympusix('panel_lights',0);       % switch off lights on the I3-TPC touch panel

olympusix('objective')             % get position of the current objective (number in revolver)
olympusix('objective',3);          % selects the third objective in the revolver as the current one

olympusix('lightpath',1);          % set lightpath to 100% left camera port
olympusix('lightpath',3);          % set lightpath to 100% eye piece
olympusix('lightpath')             % get current lightpath mode

olympusix('L 0,0');                % Olympus SDK command - logout from microscope frame
olympusix('close');                % close connection to the IX83 frame
clear olympusix                    % clear Olympus mex driver from memory
~~~