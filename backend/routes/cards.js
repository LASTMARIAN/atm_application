var express = require('express');
const cardsModel = require('../models/card_model');
const accountsModel = require('../models/account_model');
const customersModel = require('../models/customers_model');
const axios = require('axios');
var bcrypt = require('bcrypt');
var jwt = require('jsonwebtoken');
var db = require('../db');
const { verifyToken } = require('../verifyToken')

var router = express.Router();
const saltRounds = 10;
const JWT_SECRET = '1234567890';

router.post('/auth', async (req, res) => {
    console.log('Processing /cards/auth request...');
    const { card_number, pin_code } = req.body;
    if (!card_number || !pin_code) {
        console.log('Missing card_number or pin_code');
        return res.status(400).json({ error: 'card_number and pin_code are required' });
    }

    try {
        console.log('Fetching card for card_number:', card_number);
        const card = await cardsModel.getOne(card_number);
        if (!card) {
            console.log('Card not found');
            return res.status(404).json({ error: 'Korttia ei löydy' });
        }

        console.log('Checking if card is blocked:', card.is_blocked);
        if (card.is_blocked) {
            console.log('Card is blocked');
            return res.status(403).json({ error: 'Kortti on estetty' });
        }

        console.log('Verifying PIN...');
        const pinMatch = await bcrypt.compare(pin_code, card.pin_hash);
        if (!pinMatch) {
            console.log('Invalid PIN');

            const newFailedAttempts = (card.failed_pin_attempts || 0) + 1;
            console.log('Incrementing failed_pin_attempts to:', newFailedAttempts);


            const isBlocked = newFailedAttempts >= 3 ? 1 : 0;
            console.log('Setting is_blocked to:', isBlocked);


            await cardsModel.update(card_number, {
                failed_pin_attempts: newFailedAttempts,
                is_blocked: isBlocked
            });


            if (isBlocked) {
                console.log('Card is now blocked due to 3 failed attempts');
                return res.status(403).json({ error: 'Kortti on estetty' });
            }

            return res.status(403).json({ error: 'Väärä PIN-koodi' });
        }


        console.log('PIN correct, resetting failed_pin_attempts and is_blocked...');
        await cardsModel.update(card_number, {
            failed_pin_attempts: 0,
            is_blocked: 0
        });

        console.log('Fetching account for account_id:', card.account_id);
        const account = await accountsModel.getOne(card.account_id);
        if (!Array.isArray(account) || account.length === 0) {
            console.log('Account not found');
            return res.status(404).json({ error: 'Account not found' });
        }

        console.log('Fetching customer for customer_id:', account[0].customer_id);
        const customer = await customersModel.getOne(account[0].customer_id);
        if (!customer) {
            console.log('Customer not found');
            return res.status(404).json({ error: 'Customer not found' });
        }

        console.log('Generating JWT token...');
        const token = jwt.sign(
            { account_id: card.account_id },
            JWT_SECRET,
            { expiresIn: '1h' }
        );

        res.cookie('token', token, {
            httpOnly: true,
            secure: process.env.NODE_ENV === 'production',
            sameSite: 'strict',
            maxAge: 60 * 60 * 1000
        });

        console.log('Authentication successful, sending response...');
        res.status(200).json({
            success: true,
            customer: {
                first_name: customer.first_name,
                last_name: customer.last_name
            },
            account_id: card.account_id,
            card_type: card.card_type
        });
    } catch (error) {
        console.error('Error in /cards/auth:', error.message);
        res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
    }
});

router.get('/get_cards', verifyToken, async (req, res) => {
    cardsModel.getAll((error, cardRows) => {
        if (error) {
            console.error('Virhe:', error);
            return res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
        }

        res.status(200).json(cardRows.map(row => ({
            card_id: row.card_id,
            card_number: row.card_number,
            account_id: row.account_id,
            card_type: row.card_type,
            credit_limit: row.credit_limit ? parseFloat(row.credit_limit) : null
        })));
    });
});

router.post('/create_card', (req, res) => {
    const { card_number, pin_code, account_id, card_type, credit_limit } = req.body;

    if (!card_number || !pin_code || !account_id || !card_type) {
        return res.status(400).json({ error: 'card_number, pin_code, account_id ja card_type ovat pakollisia' });
    }

    bcrypt.hash(pin_code, saltRounds, (hashErr, pinHash) => {
        if (hashErr) {
            console.error('Virhe:', hashErr);
            return res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
        }

        const newCard = {
            card_number,
            pin_hash: pinHash,
            account_id,
            card_type,
            credit_limit: credit_limit || null
        };

        cardsModel.add(newCard, (error, result) => {
            if (error) {
                console.error('Virhe:', error);
                return res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
            }

            res.status(201).json({
                message: 'Kortti luotu onnistuneesti',
                card_id: result.insertId,
                card_number
            });
        });
    });
});

router.put('/:cardNumber', verifyToken, (req, res) => {
    const cardNumber = req.params.cardNumber;
    const { pin_code, account_id, card_type, credit_limit } = req.body;

    if (!pin_code || !account_id || !card_type) {
        return res.status(400).json({ error: 'pin_code, account_id ja card_type ovat pakollisia' });
    }

    bcrypt.hash(pin_code, saltRounds, (hashErr, pinHash) => {
        if (hashErr) {
            console.error('Virhe:', hashErr);
            return res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
        }

        const updatedCard = {
            pin_hash: pinHash,
            account_id,
            card_type,
            credit_limit: credit_limit || null
        };

        cardsModel.update(cardNumber, updatedCard, (error, result) => {
            if (error) {
                console.error('Virhe:', error);
                return res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
            }

            if (result.affectedRows === 0) {
                return res.status(404).json({ error: 'Korttia ei löydy' });
            }

            res.status(200).json({ message: 'Kortti päivitetty onnistuneesti' });
        });
    });
});

router.delete('/:cardNumber', verifyToken, (req, res) => {
    const cardNumber = req.params.cardNumber;

    cardsModel.delete(cardNumber, (error, result) => {
        if (error) {
            console.error('Virhe:', error);
            return res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
        }

        if (result.affectedRows === 0) {
            return res.status(404).json({ error: 'Korttia ei löydy' });
        }

        res.status(200).json({ message: 'Kortti poistettu onnistuneesti' });
    });
});

module.exports = router;