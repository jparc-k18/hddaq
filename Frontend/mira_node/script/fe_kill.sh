#!/bin/sh

pkill -f "/bin/sh.*frontend.sh"
if [ $? = 0 ]; then
    echo "# frontend.sh : killed"
else
    echo "# frontend.sh : no process"
fi

pkill -f "frontend($| )"
if [ $? = 0 ]; then
    echo "# frontend    : killed"
else
    echo "# frontend    : no process"
fi

pkill -f "/bin/sh.*message.sh"
if [ $? = 0 ]; then
    echo "# message.sh  : killed"
else
    echo "# message.sh  : no process"
fi

pkill -f "msgd($| )"
if [ $? = 0 ]; then
    echo "# msgd        : killed"
else
    echo "# msgd        : no process"
fi
