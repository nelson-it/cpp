create schema mne_catalog; 
grant usage on schema mne_catalog to public;

/******************************/
/* Unique Indentifier erzeugen*/
/******************************/

DROP TABLE mne_catalog.id_count;
CREATE TABLE mne_catalog.id_count
    ( index INTEGER CHECK ( index = 0 ),
      id    INTEGER NOT NULL,
      lasttime INTEGER );
GRANT ALL ON mne_catalog.id_count to PUBLIC;

INSERT INTO mne_catalog.id_count VALUES ( 0, 32768, 0 );

CREATE OR REPLACE FUNCTION
    mne_catalog.mk_id()
    RETURNS VARCHAR AS $$
DECLARE 
    rec RECORD;
    str TEXT;
BEGIN
    
    SELECT
    INTO
        rec
        CAST(FLOOR(EXTRACT(EPOCH FROM CURRENT_TIMESTAMP)) AS INTEGER)
	    AS acttime,
        id,
        lasttime
    FROM
        mne_catalog.id_count;

    IF ( rec.lasttime < rec.acttime ) OR rec.id < 32768 OR rec.id > 65535 THEN
        rec.id = 32768;
    ELSE
        rec.id = rec.id + 1;
    END IF;

    EXECUTE 'UPDATE mne_catalog.id_count SET id = '
            || quote_literal(rec.id)
	    || ', lasttime = '
	    || quote_literal(rec.acttime);

    return TO_HEX(rec.acttime) || TO_HEX(rec.id);
END;
$$ LANGUAGE plpgsql;

GRANT EXECUTE ON
    FUNCTION mne_catalog.mk_id()
    TO PUBLIC;


/****************************/
/* Historyfunction erzeugen */
/****************************/

CREATE OR REPLACE FUNCTION
    mne_catalog.history_create(schema varchar, tabname varchar, refname varchar,
                               historytab varchar ) 
    RETURNS INTEGER AS $$
DECLARE
    
    n RECORD;
    str text := '';
    cols varchar[] = '{}';
    anzahl_cols integer := 0;
    modify_str varchar = 'CAST(EXTRACT(EPOCH FROM current_timestamp) AS int4)';
    i integer;
    trigname varchar;
    fullname varchar;
    funcname varchar;
BEGIN
     str =  'SELECT a.attname as attname '
         || 'FROM '
	 || '  pg_catalog.pg_class c,'
	 || '  pg_catalog.pg_namespace n,'
	 || '  pg_catalog.pg_attribute a '
         || 'WHERE '
         || '  attrelid = c.oid '
         || '  AND a.attisdropped = false '
         || '  AND attnum > 0::pg_catalog.int2 '
         || '  AND n.nspname = ''' || schema || ''' '
         || '  AND c.relkind = ''r'' '
         || '  AND c.relnamespace = n.oid '
         || '  AND c.relname = ''' || tabname || ''' ';

    FOR n IN EXECUTE str LOOP
        IF n.attname = 'modifydate' THEN
	    modify_str = 'quote_literal(modrecord.modifydate)';
	ELSIF n.attname != 'createuser' AND
	      n.attname != 'createdate' AND
	      n.attname != 'modifyuser' AND
	      n.attname != refname THEN
	    cols[anzahl_cols] = n.attname;
	    anzahl_cols = anzahl_cols + 1;
	END IF;
    END LOOP;

    funcname = schema || '.' || 'mne_history_' || tabname || '()';
    str = 'CREATE OR REPLACE FUNCTION ' || funcname
       || '  RETURNS TRIGGER AS $t$'
       || '  DECLARE '
       || '    str varchar := '''';'
       || '    oldval varchar := '''';'
       || '    newval varchar := '''';'
       || '    modrecord RECORD;'
       || '  BEGIN ';


    FOR i IN 0 .. anzahl_cols-1 LOOP

       str = str 
       || '    IF ( TG_OP = ''DELETE'' ) THEN '
       || '      modrecord = OLD;'
       || '    ELSE '
       || '      modrecord = NEW;'
       || '    END IF;'

       || '    newval = modrecord.' || cols[i] || ';'
       || '    oldval = OLD.' || cols[i] || ';'

       || '  IF ( newval IS NULL ) THEN newval = ''''; END IF;'
       || '  IF ( oldval IS NULL ) THEN oldval = ''''; END IF;'

       || '  IF ( TG_OP = ''DELETE'' OR newval <> oldval ) THEN '
       || '    str = ''INSERT INTO ' || historytab || ' '''
       || '        || ''( operation, createdate, createuser, refid, refcol, '''
       || '        ||    ''  schema, tabname, colname, '''
       || '        ||    ''  oldvalue, newvalue ) '''
       || '        || ''SELECT '' || quote_literal(TG_OP)'
       || '        ||    '','' || ' || modify_str || ' || '', user, '''
       || '        ||    quote_literal(modrecord.' || refname || ') || '', '''
       || '        ||    '' ''''' || refname || ''''', '''
       || '        ||    '' ''''' || schema || ''''', '
       || '              ''''' || tabname || ''''', '''''

       || cols[i] 

       || ''''', '''
       || '      ||    quote_literal(oldval) || '','' || quote_literal(newval);'

       || '  EXECUTE str;'
       || '  END IF; ';

    END LOOP;

       str = str || '  RETURN NULL;'

       || '  END;'
       || '$t$ LANGUAGE plpgsql;';

    EXECUTE str;
    EXECUTE 'GRANT EXECUTE ON FUNCTION ' || funcname || ' TO public';

    trigname = 'mne_history ';
    fullname = schema || '.' || tabname;

    BEGIN
	EXECUTE 'DROP TRIGGER ' || trigname || ' ON ' || fullname;
    EXCEPTION WHEN UNDEFINED_OBJECT THEN END;

   str = 'CREATE TRIGGER ' || trigname
           || ' AFTER UPDATE OR DELETE on ' || fullname || ' ' 
           || ' FOR EACH ROW EXECUTE PROCEDURE ' || funcname;
    EXECUTE str;

    RETURN 1;
END;
$$ LANGUAGE plpgsql;

/****************************/
/* Historyfunction löschen  */
/****************************/

CREATE OR REPLACE FUNCTION
    mne_catalog.history_drop(schema varchar, tabname varchar ) 
    RETURNS INTEGER AS $$
DECLARE
    trigname varchar;
    fullname varchar;
    funcname varchar;
BEGIN

    fullname = schema || '.' || tabname;
    trigname = 'mne_history';
    funcname = schema || '.' || 'mne_history_' || tabname || '()';

    BEGIN
	EXECUTE 'DROP TRIGGER ' || trigname || ' ON ' || fullname;
    EXCEPTION WHEN UNDEFINED_OBJECT THEN END;

    BEGIN
    EXECUTE 'DROP FUNCTION ' || funcname;
    EXCEPTION WHEN UNDEFINED_OBJECT THEN END;

    RETURN 1;
END;
$$ LANGUAGE plpgsql;

/****************************/
/* History Funktione lesen  */
/****************************/

CREATE OR REPLACE FUNCTION
    mne_catalog.history_check(schema varchar, tabname varchar ) 
    RETURNS INT AS $$
DECLARE
    str text;
    trigname varchar;
    fullname varchar;
    funcname varchar;
    rec RECORD;
BEGIN

    fullname = schema || '.' || tabname;
    trigname = 'mne_history';
    funcname = schema || '.' || 'mne_history_' || tabname || '()';

     SELECT INTO rec tgrelid 
         FROM 
	 pg_catalog.pg_trigger t,
	 pg_catalog.pg_class c,
	 pg_catalog.pg_namespace n
         WHERE 
         tgrelid = c.oid 
         AND n.nspname = schema
         AND c.relkind = 'r' 
         AND c.relnamespace = n.oid 
         AND c.relname = tabname
	 AND t.tgname = 'mne_history';

    IF ( NOT FOUND ) THEN
	RETURN 0;
    ELSE
        RETURN 1;
    END IF;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE VIEW mne_catalog.historycheck AS 
 SELECT tables.table_schema::character varying AS "schema", tables.table_name::character varying AS "table", mne_catalog.history_check(tables.table_schema::character varying, tables.table_name::character varying) AS showhistory
   FROM information_schema.tables;

ALTER TABLE mne_catalog.historycheck OWNER TO adminsys;
GRANT SELECT ON TABLE mne_catalog.historycheck TO public;
GRANT SELECT, UPDATE, INSERT, DELETE, REFERENCES, TRIGGER ON TABLE mne_catalog.historycheck TO adminsys WITH GRANT OPTION;

GRANT EXECUTE ON
    FUNCTION mne_catalog.history_create(schema varchar, tabname varchar,
                                        refname varchar, historytab varchar )
    TO GROUP adminsys, GROUP adminhistory;
GRANT EXECUTE ON
    FUNCTION mne_catalog.history_drop(schema varchar, tabname varchar )
    TO GROUP adminsys, GROUP adminhistory;
GRANT EXECUTE ON
    FUNCTION mne_catalog.history_check(schema varchar, tabname varchar )
    TO public;

/* ********************************************************************
   Usertimecolumns
   ******************************************************************** */
CREATE OR REPLACE FUNCTION mne_catalog.usertimecolumn_check("schema" character varying, tabname character varying)
  RETURNS integer AS
$BODY$
DECLARE
    rec RECORD;
BEGIN

     SELECT INTO rec count(*) as num 
       FROM
         information_schema.columns
       WHERE
         table_schema = schema
         AND table_name = tabname
         AND column_name in ( 'createdate', 'createuser', 'modifydate', 'modifyuser' );
    
   IF ( rec.num != 4 ) THEN
	RETURN 0;
    ELSE
        RETURN 1;
    END IF;
END;
$BODY$
  LANGUAGE 'plpgsql' VOLATILE;
ALTER FUNCTION mne_catalog.usertimecolumn_check("schema" character varying, tabname character varying) OWNER TO "adminsys";
GRANT EXECUTE ON FUNCTION mne_catalog.usertimecolumn_check("schema" character varying, tabname character varying) TO public;

CREATE OR REPLACE VIEW mne_catalog.usertimecolumncheck AS 
 SELECT tables.table_schema::character varying AS "schema", tables.table_name::character varying AS "table", mne_catalog.usertimecolumn_check(tables.table_schema::character varying, tables.table_name::character varying) AS showhistory
   FROM information_schema.tables;

ALTER TABLE mne_catalog.usertimecolumncheck OWNER TO adminsys;
GRANT SELECT ON TABLE mne_catalog.usertimecolumncheck TO public;
GRANT SELECT, UPDATE, INSERT, DELETE, REFERENCES, TRIGGER ON TABLE mne_catalog.usertimecolumncheck TO adminsys WITH GRANT OPTION;

/* ********************************************************************
   Settings
   ******************************************************************** */
CREATE OR REPLACE FUNCTION
    mne_catalog.start_session(app_schema varchar)
    RETURNS varchar AS $$
DECLARE 
    str varchar;
    val interval;
BEGIN
    BEGIN
	CREATE LOCAL TEMP TABLE mne_settings 
	    (
	    datestyle     varchar NOT NULL,
	    timestyle     varchar NOT NULL,
	    datetimestyle varchar NOT NULL,
	    timezone      varchar NOT NULL
	    );
    EXCEPTION WHEN DUPLICATE_TABLE THEN NULL;
    END;

    EXECUTE 'INSERT INTO mne_settings
    SELECT dbdatestyle, dbtimestyle, dbdatetimestyle, timezone
    FROM ' || app_schema || '.userpref
    WHERE username = current_user';

    SELECT timezone INTO str FROM mne_settings;
    EXECUTE 'SET SESSION TIME ZONE ''' || str || '''';

    return 'ok';
END;
$$ LANGUAGE plpgsql;

GRANT EXECUTE ON
    FUNCTION mne_catalog.start_session(app_schema varchar)
    TO PUBLIC;

/* ********************************************************************
   Zeitfunktionen
   ******************************************************************** */
CREATE OR REPLACE FUNCTION
    mne_catalog.epoch2date(timevalue int4)
    RETURNS varchar AS $$
DECLARE 
    str text;
    val interval;
BEGIN
    
    SELECT to_char(( TIMESTAMP WITH TIME ZONE '1-1-1970 GMT'
            + CAST ( ( TO_CHAR(timevalue, '9999999999999999999') || ' sec')
	             as INTERVAL) ) 
            , datestyle ) from mne_settings
	    INTO str;

    return str;
END;
$$ LANGUAGE plpgsql;

GRANT EXECUTE ON
    FUNCTION mne_catalog.epoch2date(timevalue int4)
    TO PUBLIC;

CREATE OR REPLACE FUNCTION
    mne_catalog.epoch2time(timevalue int4)
    RETURNS varchar AS $$
DECLARE 
    str text;
    val interval;
BEGIN
    
    SELECT to_char(( TIMESTAMP WITH TIME ZONE '1-1-1970 GMT'
            + CAST ( ( TO_CHAR(timevalue, '9999999999999999999') || ' sec')
	             as INTERVAL) ) 
            , timestyle ) from mne_settings
	    INTO str;

    return str;
END;
$$ LANGUAGE plpgsql;

GRANT EXECUTE ON
    FUNCTION mne_catalog.epoch2time(timevalue int4)
    TO PUBLIC;

CREATE OR REPLACE FUNCTION
    mne_catalog.epoch2datetime(timevalue int4)
    RETURNS varchar AS $$
DECLARE 
    str text;
    val interval;
BEGIN
    
    SELECT to_char(( TIMESTAMP WITH TIME ZONE '1-1-1970 GMT'
            + CAST ( ( TO_CHAR(timevalue, '9999999999999999999') || ' sec')
	             as INTERVAL) ) 
            , datetimestyle ) from mne_settings
	    INTO str;

    return str;
END;
$$ LANGUAGE plpgsql;

GRANT EXECUTE ON
    FUNCTION mne_catalog.epoch2datetime(timevalue int4)
    TO PUBLIC;

/* ************************************************************************ */
/* Sichten und Prozeduren für sqlproc                                       */
/* ************************************************************************ */
CREATE OR REPLACE FUNCTION
    mne_catalog.pgplsql_proc_create(name varchar, rettyp varchar,
                                    text varchar, asowner bool )
    RETURNS VARCHAR AS $$
   
/* WARNUNG - DIESE FUNKTION NICHT EDITIEREN */

DECLARE
    stm VARCHAR;
BEGIN

    stm =  'CREATE OR REPLACE FUNCTION ' 
        || name
	|| ' RETURNS '
	|| rettyp
	|| ' AS \$\$ '
	|| text
	|| ' \$\$ LANGUAGE plpgsql';

   if asowner = true THEN stm = stm || ' SECURITY DEFINER'; END IF;

    EXECUTE stm;
    return 'ok';

END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION
    mne_catalog.pgplsql_proc_drop(name varchar)
    RETURNS VARCHAR AS $$
DECLARE
    stm VARCHAR;
BEGIN

    stm =  'DROP FUNCTION ' || name;
    EXECUTE stm;

    return 'ok';

END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION
    mne_catalog.pgplsql_proc_access_add(proc varchar, name varchar)
    RETURNS VARCHAR
    AS
$$
DECLARE
    stm varchar;
BEGIN
    stm = 'GRANT EXECUTE ON FUNCTION ' || proc || ' TO ';

    IF name = '' THEN
        stm = stm ||  ' PUBLIC ';
    ELSE
        stm = stm ||  name;
    END IF;
    EXECUTE stm;

    return 'ok';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION
    mne_catalog.pgplsql_proc_access_drop(proc varchar, name varchar)
    RETURNS VARCHAR
    AS
$$
DECLARE
    stm varchar;
BEGIN
    stm = 'REVOKE EXECUTE ON FUNCTION ' || proc || ' FROM ';

    IF name = '' THEN
        stm = stm ||  ' PUBLIC ';
    ELSE
        stm = stm ||  name;
    END IF;
    EXECUTE stm;

    return 'ok';
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION mne_catalog.pgplsql_fargs(oidin OID) RETURNS VARCHAR AS $$
DECLARE
    stm VARCHAR;
    str VARCHAR;
    komma VARCHAR;
    rec RECORD;
BEGIN
    str = '(';

    stm =  'SELECT '
        || '  p.proargnames[count.i + 1] as argname, '
        || '  count.i as num, '
        || '  t.typname '
	|| 'FROM '
	|| ' pg_proc p, pg_type t, '
        || ' ( SELECT pc.proname, pc.oid, generate_series(0, pc.pronargs - 1) AS i '
        || '   FROM pg_proc pc ) AS count '
        || 'WHERE '
	|| '    count.oid = p.oid '
	|| 'AND t.oid = p.proargtypes[count.i] '
	|| 'AND count.oid = ' || CAST(oidin AS VARCHAR);

    komma = '';
    FOR rec IN EXECUTE stm LOOP
        str = str || komma || rec.argname || ' ' || rec.typname;
	komma = ', ';
    END LOOP;
    str = str || ')';

    return str;
END;
$$ LANGUAGE plpgsql;

/* ************************************************************************ */
/* View zum Tabllen editieren                                               */
/* ************************************************************************ */
CREATE OR REPLACE VIEW mne_catalog.fkey AS 
 SELECT con.conname AS name, ns.nspname AS "schema", cl.relname AS "table", attr.attname AS "column", rns.nspname AS fschema, rcl.relname AS ftable, rattr.attname AS fcolumn, count.i AS "position"
   FROM pg_constraint con, pg_namespace ns, pg_class cl, pg_attribute attr, pg_namespace rns, pg_class rcl, pg_attribute rattr, ( SELECT generate_series(1, 10) AS i) count
  WHERE con.contype = 'f'::"char" AND ns.oid = con.connamespace AND con.conrelid = cl.oid AND attr.attrelid = cl.oid AND attr.attnum = con.conkey[count.i] AND con.confrelid = rcl.oid AND rattr.attrelid = rcl.oid AND rattr.attnum = con.confkey[count.i] AND rcl.relnamespace = rns.oid;

ALTER TABLE mne_catalog.fkey OWNER TO adminsys;
GRANT SELECT, UPDATE, INSERT, DELETE, REFERENCES, TRIGGER ON TABLE mne_catalog.fkey TO adminsys;
GRANT SELECT ON TABLE mne_catalog.fkey TO public;
