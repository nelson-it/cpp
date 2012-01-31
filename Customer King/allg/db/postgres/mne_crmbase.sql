CREATE OR REPLACE FUNCTION                            
    mne_crmbase.offer2order(offerid varchar)
    RETURNS VARCHAR AS $$
DECLARE 
    rec RECORD;
    acttime INTEGER;
    orderid TEXT;
    str TEXT;
    typ TEXT[2];
    i INTEGER;
BEGIN
    
    SELECT INTO acttime
        CAST(FLOOR(EXTRACT(EPOCH FROM CURRENT_TIMESTAMP)) AS INTEGER);

    SELECT INTO orderid mne_catalog.mk_id();

    /* Tabelle order füllen */
    /* ==================== */

    EXECUTE 'INSERT INTO mne_crmbase.order '
	     || '( orderid, refid, ownerid, ordernumber, '
	     || '  description, text, withvat, '
	     || '  createdate, createuser, modifydate, modifyuser ) '
	     || 'SELECT '
	     ||    quote_literal(orderid) 
	     || ' , refid, ownerid, offernumber, '
	     || '  description, text, withvat, '
	     || quote_literal(acttime)
	     || ', current_user, '
	     || quote_literal(acttime)
	     || ', current_user '
	     || 'FROM '
	     || 'mne_crmbase.offer '
	     || 'WHERE '
	     || 'offerid = ' || quote_literal(offerid);


    /* Tabelle orderproduct füllen */
    /* =========================== */

    typ = '{\'\'\\,\'option\' , \'support\' }';

    FOR i IN 1..2 LOOP
	EXECUTE 'INSERT INTO mne_crmbase.orderproduct '
	     || '( orderproductid, orderid, orderproducttype, position, count, '
	     || '  createdate, createuser, modifydate, modifyuser, '
	     || '  productcurrencyid, productdescription, productname, '
	     || '  productunit, productprice, productvat, productcurrencyrate) '
	     || ' SELECT mne_catalog.mk_id(), '
	     ||          quote_literal(orderid) || ', '
	     || '        \'\', max(t0.position), sum(t0.count), '
	     ||          to_char(acttime, '9999999999999999') || ', '
	     || '        current_user, '
	     ||         to_char(acttime, '99999999999999999') || ', '
	     || '       current_user, '
	     || '       t2.currencyid, t1.description, t1.name, '
	     || '       t2.unit, t2.unitprice, t2.vat, t3.rate_purchase '
	     || ' FROM (( mne_crmbase.offerproduct t0 LEFT JOIN '
	     || '        mne_crmbase.product t1 '
	     || '        ON ( t0.productid = t1.productid )) LEFT JOIN '
	     || '        mne_crmbase.productprice t2 '
	     || '        ON ( t1.productid = t2.productid )) LEFT JOIN '
	     || '        mne_base.currency t3 '
	     || '        ON ( t2.currencyid = t3.currencyid ) '
	     || ' WHERE t0.offerid = ' || quote_literal(offerid)
	     || '   AND t0.offerproducttype in ( ' || typ[i] || ' ) '
	     || ' GROUP BY t2.currencyid, description, name, unit, unitprice, '
	     || '          vat, t3.rate_purchase';
 
    END LOOP;

    return 'Ok';
END;
$$ LANGUAGE plpgsql;

GRANT EXECUTE ON
    FUNCTION mne_crmbase.offer2order(offerid varchar)
    TO PUBLIC;

