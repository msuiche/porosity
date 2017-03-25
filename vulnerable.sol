contract SendBalance {
    mapping ( address => uint ) userBalances ;
    bool withdrawn = false ;

    function getBalance (address u) constant returns ( uint ){
        return userBalances [u];
    }

    function addToBalance () {
        userBalances[msg.sender] += msg.value ;
    }

    function withdrawBalance (){
        if (!(msg.sender.call.gas(0x1111).value (
            userBalances [msg . sender ])())) { throw ; }
        userBalances [msg.sender ] = 0;
    }
}
