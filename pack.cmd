/* pack.cmd */

/***** start of configuration block *****/

sPackageName = 'libkai'
sVerMacro = 'KAI_VERSION'
sVerHeader = 'kai.h'

/***** end of configureation block *****/

sCmd = 'sed -n "s/^#define' sVerMacro '*\\\"\(.*\)\\\"$/\1/p"' sVerHeader
sVer = getOutput( sCmd )

'gmake distclean'
'gmake dist VER=' || sVer
'sed "s/@VER@/' || sVer || '/g"',
     sPackageName || '.txt >' sPackageName || '-' || sVer || '.txt'
'zip' sPackageName || '-' || sVer || '.zip',
      sPackageName || '-' || sVer || '.txt donation.txt'

exit 0

getOutput: procedure
    parse arg sCmd;

    nl = x2c('d') || x2c('a');

    rqNew = rxqueue('create');
    rqOld = rxqueue('set', rqNew );

    address cmd sCmd '| rxqueue' rqNew;

    sResult = '';
    do while queued() > 0
        parse pull sLine;
        sResult = sResult || sLine || nl;
    end

    call rxqueue 'Delete', rqNew;
    call rxqueue 'Set', rqOld;

    /* remove empty lines at end */
    do while right( sResult, length( nl )) = nl
        sResult = delstr( sResult, length( sResult ) - length( nl ) + 1 );
    end

    return sResult;
