% open connection
olympusix('open');
pause

% login to Olympus microscope
olympusix('L 1,0');
pause

%% commads related to Z-drive control
% enable jog wheel
olympusix('EN6 1,1');
pause

% enable TPC
olympusix('EN5 1');
pause

% enter configuration mode
olympusix('OPE 1');

% set settings for DIA - all to None
olympusix('S_U1 2,1,N,N,1,O');

% set settings for DIA - all to None
olympusix('S_U2 N,N,N,N');

% set available mirror names for Deck2
olympusix('S_MU2 N,N,N,N');

% set names of the observation modes
olympusix('S_OMCP Fluo,Scan,Cam');

% Open Command for Setting - leave configuration mode: after this Z-drive control works
olympusix('OPE 0');
pause

% focus notification on
olympusix('NFP 1');
pause

% all other notifications on 
olympusix('N 1');
pause

% host key notification on
olympusix('SK 1');
pause

% objective lens change notification
olympusix('NOB 1');
pause

% enable Escape button on TPC unit
olympusix('SKD 354,1');
pause

% Absolute movement of Z to 60.00[um]
olympusix('FG 6000');
pause

% Relative movement of Z to +10.00[um]
olympusix('FM N,1000');
pause

% Relative movement of Z to -10.00[um]
olympusix('FM F,1000');
pause

% get local coordinate
olympusix('FP?'); 
pause
%% not yet useful commands
% Get Available Unit
olympusix('U?');
pause

%% logout and de-initialization
% logout from microscope frame
olympusix('L 0,0');
pause

% close connection to the frame
olympusix('close');
pause

% clear Olympus library from memory
clear olympusix