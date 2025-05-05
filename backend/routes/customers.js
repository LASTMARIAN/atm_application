var express = require('express');
var db = require('../db');
var router = express.Router();
const customersModel = require('../models/customers_model');
const { verifyToken } = require('../verifyToken');

router.get('/get_customers', verifyToken, async (req, res) => {
    customersModel.getAll((error, customerRows) => {
        if (error) {
            console.error('Virhe:', error);
            return res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
        }

        res.status(200).json(customerRows.map(row => ({
            customer_id: row.customer_id,
            first_name: row.first_name,
            last_name: row.last_name
        })));
    });
});

router.post('/create_customers', verifyToken, (req, res) => {
    const { first_name, last_name } = req.body;

    if (!first_name || !last_name) {
        return res.status(400).json({ error: 'first_name ja last_name ovat pakollisia' });
    }

    const newCustomer = { first_name, last_name };

    customersModel.add(newCustomer, (error, result) => {
        if (error) {
            console.error('Virhe:', error);
            return res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
        }

        res.status(201).json({
            message: 'Asiakas luotu onnistuneesti',
            customer_id: result.insertId
        });
    });
});

router.put('/:customerId', verifyToken, (req, res) => {
    const customerId = req.params.customerId;
    const { first_name, last_name } = req.body;

    if (!first_name || !last_name) {
        return res.status(400).json({ error: 'first_name ja last_name ovat pakollisia' });
    }

    const updatedCustomer = { first_name, last_name };

    customersModel.update(customerId, updatedCustomer, (error, result) => {
        if (error) {
            console.error('Virhe:', error);
            return res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
        }

        if (result.affectedRows === 0) {
            return res.status(404).json({ error: 'Asiakasta ei löydy' });
        }

        res.status(200).json({ message: 'Asiakas päivitetty onnistuneesti' });
    });
});

router.delete('/:customerId', verifyToken, (req, res) => {
    const customerId = req.params.customerId;

    customersModel.delete(customerId, (error, result) => {
        if (error) {
            console.error('Virhe:', error);
            return res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
        }

        if (result.affectedRows === 0) {
            return res.status(404).json({ error: 'Asiakasta ei löydy' });
        }

        res.status(200).json({ message: 'Asiakas poistettu onnistuneesti' });
    });
});

module.exports = router;